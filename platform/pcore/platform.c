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

// platform memory regions 

/* HBM memory */
#define HBM_REGION_ADDR      	(0x0000000000000000ULL)
#define HBM_REGION_CFG_MASK  	(0xFFFFFC0000000000ULL)

/* Platform-level interrupt controller PLIC */ 
#define PLIC_REGION_ADDR  	   	(0xFFFF8FFFC0000000ULL)
#define PLIC_REGION_CFG_MASK    (0xFFFFFFFFFF000000ULL)

/* APB Subsystem Master Bridge and Cluster Control, non cacheable */
#define NC_REGION_ADDR          (0xFFFF8FFFE0000000ULL)
#define NC_REGION_CFG_MASK  	(0xFFFFFFFFF8000000ULL)

/* UART1 */
#define UART1_REGION_BASE_ADDR  (0xFFFF8FFFe0218000ULL)
#define UART1_REGION_CFG_MASK   (0xFFFFFFFFFFFFF000ULL)
/* MTIMER */
#define MTIMER_REGION_BASE_ADDR (0xFFFF8FFFE8000000ULL) // Timer, non cacheable (4 KB)
#define MTIMER_REGION_CFG_MASK  (0xFFFFFFFFFFFFF000ULL)
/* SRAM (SHMEM) */
#define SHMEM_REGION_ADDR  		(0xFFFF8FFFF8000000ULL) // Shared memory (4 KB)
#define SHMEM_REGION_MASK 		(0xFFFFFFFFFFFFF000ULL) 
// MPU section
#define CSR_MPU_SELECT 			(0xBC4)
#define CSR_MPU_CONTROL 		(0xBC5)
#define CSR_MPU_ADDRESS			(0xBC6)
#define CSR_MPU_MASK 			(0xBC7)
#define CSR_MEM_CTRL_GLOBAL		(0xBD4)

#define MEM_CTRL_GLOBAL_BIT_L1IC_ENA 	(1 << 0)
#define MEM_CTRL_GLOBAL_BIT_L1DC_ENA	(1 << 1)
#define MEM_CTRL_GLOBAL_BIT_L1IC_FLUSH	(1 << 2)
#define MEM_CTRL_GLOBAL_BIT_L1DC_FLUSH	(1 << 3)
#define MEM_CTRL_GLOBAL_L1C_ENA_BM (MEM_CTRL_GLOBAL_BIT_L1IC_ENA | MEM_CTRL_GLOBAL_BIT_L1DC_ENA)
#define MEM_CTRL_GLOBAL_L1C_FLUSH_BM (MEM_CTRL_GLOBAL_BIT_L1IC_FLUSH | MEM_CTRL_GLOBAL_BIT_L1DC_FLUSH)

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

#define FIRST_UNUSED_MPU_INDEX 		(7)
#pragma GCC push_options
#pragma GCC optimize ("O0")
/*
 * Platform early initialization.
 */
static int nxt_very_early_init()
{
/* MPU init start */

	csr_write(CSR_MEM_CTRL_GLOBAL,(MEM_CTRL_GLOBAL_L1C_ENA_BM | MEM_CTRL_GLOBAL_L1C_FLUSH_BM));

	// Default region
	csr_write(CSR_MPU_SELECT,0);
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_STRONG | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));

	// MTIMER region
	csr_write(CSR_MPU_SELECT,1);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(MTIMER_REGION_BASE_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(MTIMER_REGION_CFG_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MMIO | SCR_MPU_CTRL_MR | SCR_MPU_CTRL_MW | SCR_MPU_CTRL_VALID));

	// Non Cachable (NC)region
	csr_write(CSR_MPU_SELECT,2);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(NC_REGION_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(NC_REGION_CFG_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_STRONG | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));

	// REGION_PLIC_SETUP
	csr_write(CSR_MPU_SELECT,3);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(PLIC_REGION_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(PLIC_REGION_CFG_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_STRONG | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));

	// UART region
	csr_write(CSR_MPU_SELECT,4);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(UART1_REGION_BASE_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(UART1_REGION_CFG_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MMIO | SCR_MPU_CTRL_MR | SCR_MPU_CTRL_MW | SCR_MPU_CTRL_SR | SCR_MPU_CTRL_SW | SCR_MPU_CTRL_VALID));

	// HBM region
	csr_write(CSR_MPU_SELECT,5);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(HBM_REGION_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(HBM_REGION_CFG_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_WEAKLY | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));

	// SHMEM region
	csr_write(CSR_MPU_SELECT,6);
	csr_write(CSR_MPU_CONTROL,0);
	csr_write(CSR_MPU_ADDRESS,(SHMEM_REGION_ADDR >> 2));
	csr_write(CSR_MPU_MASK,(SHMEM_REGION_MASK >> 2));
	csr_write(CSR_MPU_CONTROL,(SCR_MPU_CTRL_MT_STRONG | SCR_MPU_CTRL_ALL | SCR_MPU_CTRL_VALID));

	// Last region
	csr_write(CSR_MPU_SELECT,FIRST_UNUSED_MPU_INDEX);
	csr_write(CSR_MPU_CONTROL,0);

/* MPU init end */
	return 0;
}

#pragma GCC pop_options
/*
 * Platform early initialization.
 */
static int platform_early_init(bool cold_boot)
{
	return 0;
}

/*
 * Platform final initialization.
 */
static int platform_final_init(bool cold_boot)
{
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
	return 0;
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.nascent_init   = nxt_very_early_init,
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
