/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/irqchip/plic.h>

/*
 * Include these files as needed.
 * See config.mk PLATFORM_xxx configuration parameters.
 */
#include <sbi/sbi_console.h>

/*
 * Cluster control register MHART4 (0xFFFF8FFFE0400030) holds MHARTID for SCR7 cores.
 * Opensbi implementation is using CSR_MHARTID register which starts from 32 for CORE0
 * As a w/a start count from 32 to avoid changing orig implementation.
*/
#define PLATFORM_HART_COUNT 33 // allow boot only for scr7 core 0 (hartid=32)

// sram log ring
#define PLATFORM_LOG_RING_SIZE 0x1000
#define PLATFORM_LOG_RING_END_ADDRESS 0xffff8ffff8200000
#define SCR7_TEST_PRINT "\x1b[96m[SCR7 opensbi] init done.\n"

// fundtions 
static void *ns_memcpy(void *dst, const void *src, uint32_t count);

// Structs
struct platform_log_ring {
    uint32_t rsvd_lock;
    uint32_t head;
    char log[PLATFORM_LOG_RING_SIZE];
};

/* UART1 */
#define UART1_BASE_ADDR (0xffff8fffe0218000)
#define UART1_SIZE (0x1000)
#define UART1_FLAGS (SBI_DOMAIN_MEMREGION_READABLE | SBI_DOMAIN_MEMREGION_WRITEABLE | SBI_DOMAIN_MEMREGION_MMIO)
#define UART1_FREQ (50000000)
#define UART1_BAUD (115200)
#define UART1_REG_SHIFT (2)
#define UART1_REG_WIDTH (4)

// MPU section
#define CSR_MPU_SELECT 			(0xBC4)
#define CSR_MPU_CONTROL 		(0xBC5)
#define CSR_MPU_ADDRESS			(0xBC6)
#define CSR_MPU_MASK 			(0xBC7)
#define CSR_MEM_CTRL_GLOBAL		(0xBD4)
#define HBM_REGION_ADDR      	(0x000000)
#define HBM_REGION_CFG_MASK  	(0xFFFFFFFFFFF00000ULL)

#define SCR_MPU_CTRL_VALID          (1 << 0)  // Current entry is enabled
#define SCR_MPU_CTRL_MR             (1 << 1)  // M-Mode reads are permitted
#define SCR_MPU_CTRL_MW             (1 << 2)  // M-Mode writes are permitted
#define SCR_MPU_CTRL_MX             (1 << 3)  // M-Mode execution is permitted
#define SCR_MPU_CTRL_UR             (1 << 4)  // U-Mode reads are permitted
#define SCR_MPU_CTRL_UW             (1 << 5)  // U-Mode writes are permitted
#define SCR_MPU_CTRL_UX             (1 << 6)  // U-Mode execution is permitted
#define SCR_MPU_CTRL_SR             (1 << 7)  // S-Mode reads are permitted
#define SCR_MPU_CTRL_SW             (1 << 8)  // S-Mode writes are permitted
#define SCR_MPU_CTRL_SX             (1 << 9)  // S-Mode execution is permitted
#define SCR_MPU_CTRL_MT_MASK        (3 << 16) // Unused
#define SCR_MPU_CTRL_MT_WEAKLY      (0 << 16) // 17..16 Cacheable, weakly-ordered
#define SCR_MPU_CTRL_MT_STRONG      (1 << 16) // 17..16 Non-cacheable, strong-ordered
#define SCR_MPU_CTRL_MT_WEAKLY_NC   (2 << 16) // 17..16 Non-cacheable, weakly-ordered
#define SCR_MPU_CTRL_MMIO	        (3 << 16) // 17..16 CPU configuration memory-mapped I/O, noncacheable, strong-ordered
#define SCR_MPU_CTRL_LOCK           (1 << 31) // Current entry is locked
#define SCR_MPU_CTRL_MA             (SCR_MPU_CTRL_MR | SCR_MPU_CTRL_MW | SCR_MPU_CTRL_MX)
#define SCR_MPU_CTRL_SA             (SCR_MPU_CTRL_SR | SCR_MPU_CTRL_SW | SCR_MPU_CTRL_SX)
#define SCR_MPU_CTRL_UA             (SCR_MPU_CTRL_UR | SCR_MPU_CTRL_UW | SCR_MPU_CTRL_UX)
#define SCR_MPU_CTRL_ALL            (SCR_MPU_CTRL_MA | SCR_MPU_CTRL_SA | SCR_MPU_CTRL_UA)
#define MEM_CTRL_GLOBAL_BIT_L1IC_ENA 	(1 << 0)
#define MEM_CTRL_GLOBAL_BIT_L1DC_ENA	(1 << 1)
#define MEM_CTRL_GLOBAL_BIT_L1IC_FLUSH	(1 << 2)
#define MEM_CTRL_GLOBAL_BIT_L1DC_FLUSH	(1 << 3)
#define MEM_CTRL_GLOBAL_L1C_ENA_BM (MEM_CTRL_GLOBAL_BIT_L1IC_ENA | MEM_CTRL_GLOBAL_BIT_L1DC_ENA)
#define MEM_CTRL_GLOBAL_L1C_FLUSH_BM (MEM_CTRL_GLOBAL_BIT_L1IC_FLUSH | MEM_CTRL_GLOBAL_BIT_L1DC_FLUSH)

static const struct {
	unsigned long base, size, flags;
} platform_memory_regions[] = {
	{ UART1_BASE_ADDR, UART1_SIZE, UART1_FLAGS },
};

/*
 * Platform early initialization.
 */
static int platform_very_early_init()
{
	return 0;
}

/*
 * Platform early initialization.
 */
static int nxt_early_init(bool cold_boot)
{
	struct sbi_domain_memregion region;
	int ret;

	for (unsigned int i = 0; i < array_size(platform_memory_regions); i++) {
		sbi_domain_memregion_init(platform_memory_regions[i].base,
					  platform_memory_regions[i].size,
					  platform_memory_regions[i].flags,
					  &region);

		ret = sbi_domain_root_add_memregion(&region);
		if (ret)
			return ret;
	}

	// Default region
	csr_write(CSR_MPU_SELECT,0);
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_STRONG | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));
	
	// HBM region
	csr_write(CSR_MPU_SELECT,1);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(HBM_REGION_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(HBM_REGION_CFG_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_STRONG | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));

	// Last region
	csr_write(CSR_MPU_SELECT,2);
	csr_write(CSR_MPU_CONTROL,0);

	return 0;
}

/*
 * Platform final initialization.
 */
static int platform_final_init(bool cold_boot)
{
	// init done print to SRAM
	struct platform_log_ring *ring = (struct platform_log_ring *)(PLATFORM_LOG_RING_END_ADDRESS - PLATFORM_LOG_RING_SIZE - sizeof(uint32_t) - sizeof(uint32_t));
	ring->head = 0;
	const char test_print[]  = SCR7_TEST_PRINT;
	ns_memcpy(ring->log, test_print, sizeof(test_print));
	ring->head = sizeof(test_print);

	sbi_puts("******************Hello from ABRA!*************************\n");
	return 0;
}

/*
 * Initialize the platform console.
 */
static int nxt_console_init(void)
{
	return uart8250_init(UART1_BASE_ADDR,
			     UART1_FREQ,
			     UART1_BAUD,
			     UART1_REG_SHIFT,
			     UART1_REG_WIDTH, 0);
}

/*
 * Initialize the platform interrupt controller for current HART.
 */
static int platform_irqchip_init(bool cold_boot)
{
	return 0;
}

/*
 * Initialize IPI for current HART.
 */
static int platform_ipi_init(bool cold_boot)
{
	return 0;
}

/*
 * Initialize platform timer for current HART.
 */
static int platform_timer_init(bool cold_boot)
{
	return 0;
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.nascent_init   = platform_very_early_init,
	.early_init		= nxt_early_init,
	.final_init		= platform_final_init,
	.console_init	= nxt_console_init,
	.irqchip_init		= platform_irqchip_init,
	.ipi_init		= platform_ipi_init,
	.timer_init		= platform_timer_init,
};

const struct sbi_platform platform = {
	.opensbi_version	= OPENSBI_VERSION,
	.platform_version	= SBI_PLATFORM_VERSION(0x07, 0x00),
	.name			= "NextSilicon NSC v1",
	.features		= SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count		= PLATFORM_HART_COUNT,
	.hart_stack_size	= SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr	= (unsigned long)&platform_ops
};

static void *ns_memcpy(void *dst, const void *src, uint32_t count)
{
    char *tmp1 = dst;
    const char *tmp2 = src;

#define DO_UINT(type)                                                                              \
    if (count >= sizeof(type) && !((uintptr_t)tmp1 % sizeof(type)) &&                              \
        !((uintptr_t)tmp2 % sizeof(type))) {                                                       \
        *(type *)tmp1 = *(type *)tmp2;                                                             \
        tmp1 += sizeof(type);                                                                      \
        tmp2 += sizeof(type);                                                                      \
        count -= sizeof(type);                                                                     \
        continue;                                                                                  \
    }

    while (count > 0) {
        DO_UINT(__uint128_t)
        DO_UINT(uint64_t)
        DO_UINT(uint32_t)
        DO_UINT(uint16_t)
        DO_UINT(uint8_t)
    }

#undef DO_UINT

    return dst;
}