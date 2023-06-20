/** @file
 * @brief MMU example
 *
 * @copyright (C) Syntacore 2022. All rights reserved.
 */

#include <sbi/riscv_asm.h>
#include <sbi/sbi_console.h>
#include <mmu.h>
#include <plf.h>

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#pragma GCC push_options
#pragma GCC optimize ("O0")

typedef struct 
{
    uintptr_t vaddr;
    uintptr_t paddr;
    int isLeaf;
} pv_addr_table_entry;

uint64_t level2[PTRS_PER_PGD] __attribute__((aligned(PAGE_SIZE)));
uint64_t level1[PTRS_PER_PGD] __attribute__((aligned(PAGE_SIZE)));
uint64_t level0[PTRS_PER_PGD] __attribute__((aligned(PAGE_SIZE)));

#define GET_UINTPTR_FROM_PTR_CAST(P) ((uintptr_t)(P))

#define GET32_UINT_FROM_PTR(P, SHIFT) ((unsigned int)((GET_UINTPTR_FROM_PTR_CAST(P) >> (SHIFT))))

#define GET32_PRINTF_PAIR(X) GET32_UINT_FROM_PTR(X, 32), GET32_UINT_FROM_PTR(X, 0)

#define VAL_256TiB (256ULL * 1024 * 1024 * 1024 * 1024ULL)
#define VAL_512GiB (512 * 1024 * 1024 * 1024ULL)
#define VAL_1GiB (1 * 1024 * 1024 * 1024ULL)
#define VAL_2MiB (2 * 1024 * 1024ULL )
#define VAL_4KiB (4 * 1024ULL )

#define ALIGN_ADDR(ADDR, ALIGN) (((ADDR) / (ALIGN)) * (ALIGN))
#define ADDR_OFFSET(ADDR, ALIGN) (((ADDR) % (ALIGN)))

#define ALIGN_ADDR_256TiB(ADDR) ALIGN_ADDR(ADDR, VAL_256TiB)
#define ALIGN_ADDR_512GiB(ADDR) ALIGN_ADDR(ADDR, VAL_512GiB)
#define ALIGN_ADDR_1GiB(ADDR) ALIGN_ADDR(ADDR, VAL_1GiB)
#define ALIGN_ADDR_2MiB(ADDR) ALIGN_ADDR(ADDR, VAL_2MiB)
#define ALIGN_ADDR_4KiB(ADDR) ALIGN_ADDR(ADDR, VAL_4KiB)

uint8_t test_array[8192] __attribute__((aligned(4096)));

#define TEST_MEM_AREA_PHYS_ADDR ALIGN_ADDR_4KiB(GET_UINTPTR_FROM_PTR_CAST(test_array))

void setup_s_mode(void)
{
    /* reset mstatus */
    csr_write(mstatus, 0);

    /* disable paging */
    csr_write(satp, 0);

    /* disable interrupts delegations */
    csr_write(mideleg, 0);

    /*
     * TLB miss exception in M-mode is generated if handling of page fault
     * exceptions is delegated to S-mode. Otherwise page fault exceptions
     * are generated in M-mode on TLB miss.
     */
    const uintptr_t exceptions =
#if 1
        (1U << 12 /* CAUSE_PAGE_FAULT_FETCH */) |
        (1U << 13 /* CAUSE_PAGE_FAULT_LOAD */)  |
        (1U << 15 /* CAUSE_PAGE_FAULT_STORE */);
#else
        0U;
#endif

    /* setup exceptions delegations */
        csr_write(medeleg, exceptions);

    /* disable interrupts in S mode */
    csr_write(stvec, 0);
    csr_write(sie, 0);
    csr_write(sip, 0);

    /* set MPP=M/S, MPIE=0, SIE=0, SPP=U */
    const unsigned long ms = csr_read(mstatus) & ~((3 << 11) | (1 << 8) | (1 << 7) | (1 << 1));
    csr_write(mstatus, ms | (1 << 11));
}

void stop(void)
{
    while (1) {
        asm volatile ("wfi" ::: "memory");
    }
}

typedef uintptr_t (*pfn_get_vpn)(uintptr_t va);

uintptr_t get_vpn2(uintptr_t va) { return VPN2(va); }
uintptr_t get_vpn1(uintptr_t va) { return VPN1(va); }
uintptr_t get_vpn0(uintptr_t va) { return VPN0(va); }

pfn_get_vpn get_vpn_arr[] = {get_vpn0, get_vpn1, get_vpn2};

void print_tables(uint64_t * table, size_t length)
{
    for(size_t i = 0; i < length; i++)
    {
        if(table[i] & PAGE_VALID) sbi_printf("%u\t0x%08x%08x\n", 
            (unsigned int )i,
            GET32_PRINTF_PAIR(table[i])
            );
    }
}

void initialize_mmu_level(pv_addr_table_entry * table, size_t table_length, 
                        int level, uintptr_t * level_table)
{
    pfn_get_vpn fnVpn = get_vpn_arr[level];
    for(size_t i = 0; i < table_length; i++)
    {
        uintptr_t pa = PTE(table[i].paddr);
        uintptr_t va = fnVpn(table[i].vaddr);
        uintptr_t attrs = (table[i].isLeaf 
                        ? (PAGE_RWX | PAGE_COMMON) 
                        : PAGE_NEXT_TABLE);
        sbi_printf("attrs: %lx\n", attrs);
        uintptr_t size = ((level ? (0x1<<(level-1)) : (0)) << LEVEL_SHIFT);  // Page size bits[8,9], 00 for 4K , 01 for 2M , 10 for 1G page levels
        level_table[va] = pa | attrs | size;
        sbi_printf("Level %d %s %u: V:0x%08x%08x"
                "\tP:0x%08x%08x"
                "\tVPN%d:0x%08x%08x"
                "\tPTE:0x%08x%08x"
                "\tFull:0x%08x%08x"
                "\n",
            level, 
            (table[i].isLeaf ? "Leaf" : "Next"),
            (unsigned int)i, 
            GET32_PRINTF_PAIR(table[i].vaddr),
            GET32_PRINTF_PAIR(table[i].paddr),
            level,
            GET32_PRINTF_PAIR(va),
            GET32_PRINTF_PAIR(pa),
            GET32_PRINTF_PAIR(level_table[va])
            );
    }    
}

void enable_mmu_sc_identity_mapping(void)
{
    memset(level2, 0x0, sizeof(level2));
    memset(level1, 0x0, sizeof(level1));
    memset(level0, 0x0, sizeof(level0));

     pv_addr_table_entry table_level2[] = 
    {
        {.vaddr=ALIGN_ADDR_1GiB(PLF_MEM_BASE),              .paddr=ALIGN_ADDR_1GiB(PLF_MEM_BASE), .isLeaf=1}, //need to align opensbi address to PLF_MEM_BASE=0x40000000
        // {.vaddr=ALIGN_ADDR_1GiB(TEST_MEM_AREA_PHYS_ADDR),   .paddr=GET_UINTPTR_FROM_PTR_CAST(&level1[0]), .isLeaf=0},
        // {.vaddr=ALIGN_ADDR_1GiB(0x40000000),              .paddr=GET_UINTPTR_FROM_PTR_CAST(&level1[0]), .isLeaf=0},
    };
    const size_t table_level2_count = sizeof(table_level2) / sizeof(table_level2[0]);

    pv_addr_table_entry table_level1[] = 
    {
        {.vaddr=ALIGN_ADDR_2MiB(PLF_MEM_BASE),              .paddr=GET_UINTPTR_FROM_PTR_CAST(&level0[0]), .isLeaf=0},
        {.vaddr=ALIGN_ADDR_2MiB(TEST_MEM_AREA_PHYS_ADDR),   .paddr=GET_UINTPTR_FROM_PTR_CAST(&level0[0]), .isLeaf=0},
        // {.vaddr=ALIGN_ADDR_2MiB(0x00008c43837fdff8ULL),     .paddr=ALIGN_ADDR_2MiB(0x00008c43837fdff8ULL), .isLeaf=1},

    };
    const size_t table_level1_count = sizeof(table_level1) / sizeof(table_level1[0]);

    pv_addr_table_entry table_level0[41];
    int i = 0;
    for(; i < 41; i++)  //opensbi code size {a80000 aa86c0}
    {
        table_level0[i].isLeaf = 1;
        table_level0[i].vaddr = ALIGN_ADDR_4KiB(PLF_MEM_BASE) + 4*i*1024;
        table_level0[i].paddr = ALIGN_ADDR_4KiB(PLF_MEM_BASE) + 4*i*1024;
    }


    const size_t table_level0_count = sizeof(table_level0) / sizeof(table_level0[0]);

    sbi_printf("table_level0_count is %u\n", (unsigned int) table_level0_count);

    initialize_mmu_level(table_level2, table_level2_count, 2, level2);
    initialize_mmu_level(table_level1, table_level1_count, 1, level1);
    initialize_mmu_level(table_level0, table_level0_count, 0, level0);


    sbi_printf("Level 2 (len=%u) 0x%08x%08x\n", (unsigned int)(sizeof(level2) / sizeof(level2[0])), GET32_PRINTF_PAIR(level2));
    print_tables(level2, sizeof(level2) / sizeof(level2[0]));

    sbi_printf("Level 1 (len=%u) 0x%08x%08x\n", (unsigned int)(sizeof(level1) / sizeof(level1[0])), GET32_PRINTF_PAIR(level1));
    print_tables(level1, sizeof(level1) / sizeof(level1[0]));

    sbi_printf("Level 0 (len=%u) 0x%08x%08x\n", (unsigned int)(sizeof(level0) / sizeof(level0[0])), GET32_PRINTF_PAIR(level0));
    print_tables(level0, sizeof(level0) / sizeof(level0[0]));


    sbi_printf("writting to satp register\n");

    uint64_t lev = GET_UINTPTR_FROM_PTR_CAST(&level2[0]);    
    uint64_t satp_val = PFN(lev) | SATP_MODE_39;

    sbi_printf("satp_val = 0x%08x%08x\n", GET32_PRINTF_PAIR(satp_val));
    csr_write(satp, satp_val);
    asm volatile ("sfence.vma" ::: "memory");

    sbi_printf("After sfence\n");
}

void exec_s_mode(void)
{
    sbi_printf("Executing S-mode!!!\n");

    sbi_printf("Starting configuring mmu\n");
    enable_mmu_sc_identity_mapping();
    sbi_printf("Finished configuring mmu!\n");
    stop();
}

int test_mmu(void)
{
    sbi_printf("Start in M-mode...\n");
    setup_s_mode();

    /* jump to S-mode */
    csr_write(mepc, exec_s_mode);
    asm volatile ("mret" ::: "memory");

    sbi_printf("Failed to jump to S-mode. Stop...\n");
    return -1;
}

#pragma GCC pop_options
