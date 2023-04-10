#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/irqchip/plic.h>

#define SCR7_HART_COUNT (4)

/* PLIC */
#define PLIC_BASE_ADDR (0xffff8fffc0000000)
#define PLIC_SIZE (0x1000000)
#define PLIC_FLAGS (SBI_DOMAIN_MEMREGION_READABLE | SBI_DOMAIN_MEMREGION_WRITEABLE | SBI_DOMAIN_MEMREGION_MMIO)
#define PLIC_NUM_SOURCES (10)

/* UART1 */
#define UART1_BASE_ADDR (0xffff8fffe0218000)
#define UART1_SIZE (0x1000)
#define UART1_FLAGS (SBI_DOMAIN_MEMREGION_READABLE | SBI_DOMAIN_MEMREGION_WRITEABLE | SBI_DOMAIN_MEMREGION_MMIO)
#define UART1_FREQ (50000000)
#define UART1_BAUD (115200)
#define UART1_REG_SHIFT (2)
#define UART1_REG_WIDTH (4)

/* MTIMER */
#define MTIMER_BASE_ADDR (0xffff8fffe8000000)
#define MTIMER_SIZE (0x1000)
#define MTIMER_FLAGS (SBI_DOMAIN_MEMREGION_READABLE | SBI_DOMAIN_MEMREGION_WRITEABLE | SBI_DOMAIN_MEMREGION_MMIO)
#define MTIMER_FREQ (50000000)
#define MTIMER_MTIME_OFFSET (0x8)
#define MTIMER_MTIME_ADDR (MTIMER_BASE_ADDR + MTIMER_MTIME_OFFSET)
#define MTIMER_CSR_MTIME_ADDR (0xBFF)
#define MTIMER_CMP_OFFSET (0x10)
#define MTIMER_CMP_ADDR (MTIMER_BASE_ADDR + MTIMER_CMP_OFFSET)
#define MTIMER_CSR_CMP_ADDR (0x7C0)

static const struct {
	unsigned long base, size, flags;
} platform_memory_regions[] = {
	{ PLIC_BASE_ADDR, PLIC_SIZE, PLIC_FLAGS },
	{ UART1_BASE_ADDR, UART1_SIZE, UART1_FLAGS },
	{ MTIMER_BASE_ADDR, MTIMER_SIZE, MTIMER_FLAGS },
};

static const struct plic_data plic = {
	.addr 		= PLIC_BASE_ADDR,
	.num_src	= PLIC_NUM_SOURCES,
};

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

	return 0;
}

static int nxt_console_init(void)
{
	return uart8250_init(UART1_BASE_ADDR,
			     UART1_FREQ,
			     UART1_BAUD,
			     UART1_REG_SHIFT,
			     UART1_REG_WIDTH, 0);
}

static int nxt_irqchip_init(bool cold_boot)
{
	if (!cold_boot)
		return SBI_ENOTSUPP;

	return plic_cold_irqchip_init(&plic);
}

static u64 nxt_mtimer_value(void)
{
	return csr_read(MTIMER_CSR_MTIME_ADDR);
}

void nxt_mtimer_event_start(u64 next_event)
{
	csr_write(MTIMER_CSR_CMP_ADDR, next_event);
}

void nxt_mtimer_event_stop(void)
{
	csr_write(MTIMER_CSR_CMP_ADDR, (u64)-1);
}

static struct sbi_timer_device platform_mtimer = {
	.name = "MTIMER",
	.timer_freq = MTIMER_FREQ,
	.timer_value = nxt_mtimer_value,
	.timer_event_start = nxt_mtimer_event_start,
	.timer_event_stop = nxt_mtimer_event_stop
};

static int nxt_timer_init(bool cold_boot)
{
	sbi_timer_set_device(&platform_mtimer);
	return 0;
}

const struct sbi_platform_operations platform_ops = {
	.early_init		= nxt_early_init,
	.console_init	= nxt_console_init,
	.irqchip_init 	= nxt_irqchip_init,
	.timer_init	   	= nxt_timer_init,
};

const struct sbi_platform platform = {
	.opensbi_version	= OPENSBI_VERSION,
	.platform_version   = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name				= "NextSilicon SCR7",
	.features			= 0,
	.hart_count			= SCR7_HART_COUNT,
	.hart_stack_size	= SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr	= (unsigned long)&platform_ops
};