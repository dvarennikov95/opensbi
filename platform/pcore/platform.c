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
#include <sbi_utils/timer/aclint_mtimer.h>

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

// platform memory regions 
#define PLATFORM_SRAM_REGION_INDEX 0
#define PLATFORM_SRAM_BASE 0xFFFF8FFFF8000000ULL
#define PLATFORM_SRAM_SIZE 0x200000
#define PLATFORM_SRAM_BANKSIZE 0x80000


/* MTIMER */
#define MTIMER_BASE_ADDR (0xffff8fffe8000000)
#define MTIMER_SIZE (0x1000)
#define MTIMER_FLAGS (SBI_DOMAIN_MEMREGION_READABLE | SBI_DOMAIN_MEMREGION_WRITEABLE | SBI_DOMAIN_MEMREGION_MMIO)
#define MTIMER_FREQ (50000000)
#define MTIMER_MTIME_OFFSET (0x8)
#define MTIMER_MTIME_ADDR (MTIMER_BASE_ADDR + MTIMER_MTIME_OFFSET)
#define MTIMER_MTIME_SIZE (0x8)
#define MTIMER_CMP_OFFSET (0x10)
#define MTIMER_CMP_ADDR (MTIMER_BASE_ADDR + MTIMER_CMP_OFFSET)
#define MTIMER_CMP_SIZE (MTIMER_SIZE - MTIMER_CMP_OFFSET)
#define MTIMER_CSR_MTIME_ADDR (0xBFF)
#define MTIMER_CSR_CMP_ADDR (0x7C0)

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

static const struct {
	unsigned long base, size, align, flags;
} platform_memory_regions[] = {
	{ PLATFORM_SRAM_BASE, PLATFORM_SRAM_SIZE, PLATFORM_SRAM_BANKSIZE,
		SBI_DOMAIN_MEMREGION_READABLE | SBI_DOMAIN_MEMREGION_WRITEABLE},
};

static struct aclint_mtimer_data mtimer = {
	.mtime_freq = MTIMER_FREQ,
	.mtime_addr = MTIMER_MTIME_ADDR,
	.mtime_size = MTIMER_MTIME_SIZE,
	.mtimecmp_addr = MTIMER_CMP_ADDR,
	.mtimecmp_size = MTIMER_CMP_SIZE,
	.first_hartid = 32,
	.hart_count = PLATFORM_HART_COUNT,
	.has_64bit_mmio = TRUE,
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
static int platform_early_init(bool cold_boot)
{
	int ret = 0;
	// add SRAM memory regions
	ret = sbi_domain_root_add_memrange(platform_memory_regions[PLATFORM_SRAM_REGION_INDEX].base,
									   platform_memory_regions[PLATFORM_SRAM_REGION_INDEX].size,
									   platform_memory_regions[PLATFORM_SRAM_REGION_INDEX].align,
									   platform_memory_regions[PLATFORM_SRAM_REGION_INDEX].flags);
	return ret;
}

/*
 * Platform final initialization.
 */
static int platform_final_init(bool cold_boot)
{
	int time = 0;
    // init done print to SRAM
	struct platform_log_ring *ring = (struct platform_log_ring *)(PLATFORM_LOG_RING_END_ADDRESS - PLATFORM_LOG_RING_SIZE - sizeof(uint32_t) - sizeof(uint32_t));
	ring->head = 0;
	const char test_print[]  = SCR7_TEST_PRINT;    
	ns_memcpy(ring->log, test_print, sizeof(test_print));
	ring->head = sizeof(test_print);

    time = (int)sbi_timer_value();
    sbi_printf("%s: %d Timer test, start time: \n", __func__, time);
    sbi_timer_delay_loop(100, 50000000, NULL,NULL);
    time = sbi_timer_value();
    sbi_printf("%s: %d Timer test, end time: \n", __func__, time);

	return 0;
}

/*
 * Initialize the platform console.
 */
static int platform_console_init(void)
{
	return 0;
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
	int ret;

	/* Example if the generic ACLINT driver is used */
	if (cold_boot) {
		ret = aclint_mtimer_cold_init(&mtimer, NULL);
		if (ret)
			return ret;
	}

	return aclint_mtimer_warm_init();
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.nascent_init   = platform_very_early_init,
	.early_init		= platform_early_init,
	.final_init		= platform_final_init,
	.console_init		= platform_console_init,
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