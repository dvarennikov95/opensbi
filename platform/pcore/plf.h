/// @file
/// @brief platform specific configurations
/// Syntacore SCR* framework
///
/// @copyright (C) Syntacore 2015-2020. All rights reserved.

#ifndef PLATFORM_VCU118_SCR7_H
#define PLATFORM_VCU118_SCR7_H

#define PLF_CORE_VARIANT_SCR 7
#define PLF_CORE_VARIANT_SCR7 1
#define PLF_CORE_VARIANT SCR7

#define PLF_IMPL_STR "Syntacore FPGA"

#if __riscv_xlen != 64
#error 64bit platform, use RV64=1 for build
#endif

// memory configuration
//----------------------
#define PLF_MEM_BASE     (0xA80000) // for sram (0xFFFF8FFFF8000000ULL)
#define PLF_MEM_SIZE     (4UL*1024*1024*1024)
#define PLF_MEM_ATTR     (SCR_MPU_CTRL_MT_WEAKLY | SCR_MPU_CTRL_ALL)
#define PLF_MEM_NAME     "DDR"

#define PLF_MMCFG_BASE   EXPAND32ADDR(0xf0040000)
#define PLF_MMCFG_SIZE   (64*1024)
#define PLF_MMCFG_ATTR   (SCR_MPU_CTRL_MT_CFG | \
                          SCR_MPU_CTRL_MR |     \
                          SCR_MPU_CTRL_MW)
#define PLF_MMCFG_NAME   "MMCFG"

#define PLF_MTIMER_BASE  PLF_MMCFG_BASE
#define PLF_L2CTL_BASE   (PLF_MMCFG_BASE + 0x1000)

#define PLF_PLIC_BASE    EXPAND32ADDR(0xfe000000)
#define PLF_PLIC_SIZE    (16*1024*1024)
#define PLF_PLIC_ATTR    (SCR_MPU_CTRL_MT_STRONG |  \
                          SCR_MPU_CTRL_MR |         \
                          SCR_MPU_CTRL_MW |         \
                          SCR_MPU_CTRL_SR |         \
                          SCR_MPU_CTRL_SW)
#define PLF_PLIC_NAME    "PLIC"

#define PLF_MMIO_BASE    EXPAND32ADDR(0xff000000)
#define PLF_MMIO_SIZE    (8*1024*1024)
#define PLF_MMIO_ATTR    (SCR_MPU_CTRL_MT_STRONG |  \
                          SCR_MPU_CTRL_MR |         \
                          SCR_MPU_CTRL_MW |         \
                          SCR_MPU_CTRL_SR |         \
                          SCR_MPU_CTRL_SW)
#define PLF_MMIO_NAME    "MMIO"

#define PLF_OCRAM_BASE   EXPAND32ADDR(0xffff0000)
#define PLF_OCRAM_SIZE   (64*1024)
#define PLF_OCRAM_ATTR   (SCR_MPU_CTRL_MT_WEAKLY | \
                          SCR_MPU_CTRL_MA | \
                          SCR_MPU_CTRL_SR)
#define PLF_OCRAM_NAME   "On-Chip RAM"

#define PLF_MEM_MAP                                                   \
    {PLF_MEM_BASE, PLF_MEM_SIZE, PLF_MEM_ATTR, PLF_MEM_NAME},         \
    {PLF_MMCFG_BASE, PLF_MMCFG_SIZE, PLF_MMCFG_ATTR, PLF_MMCFG_NAME}, \
    {PLF_PLIC_BASE, PLF_PLIC_SIZE, PLF_PLIC_ATTR, PLF_PLIC_NAME},     \
    {PLF_MMIO_BASE, PLF_MMIO_SIZE, PLF_MMIO_ATTR, PLF_MMIO_NAME},     \
    {PLF_OCRAM_BASE, PLF_OCRAM_SIZE, PLF_OCRAM_ATTR, PLF_OCRAM_NAME}

#define PLF_SMP_SUPPORT 1

//----------------------
// cache configuration
//----------------------

// min cache line
#define PLF_CACHELINE_SIZE 16
// global configuration: cachable
#define PLF_CACHE_CFG CACHE_GLBL_ENABLE

//----------------------
// MMIO configuration
//----------------------

// FPGA UART port
#define PLF_UART0_BASE   (0xFFFF8FFFe0218000ULL)
#define PLF_UART0_16550
// FPGA build ID
#define PLF_BLD_ID_ADDR  (PLF_MMIO_BASE)
// FPGA sysclk, MHz
#define PLF_SYSCLK_MHZ_ADDR (PLF_MMIO_BASE + 0x1000)
#define VarClkMHz (*(const uint32_t*)(PLF_SYSCLK_MHZ_ADDR) * 1000000)
#ifndef PLF_SYS_CLK
#define PLF_SYS_CLK VarClkMHz
#endif

// external interrupt lines
#define PLF_INTLINE_UART0   1
#define PLF_INTLINE_ETH0_RX 2
#define PLF_INTLINE_ETH0_TX 3
#define PLF_INTLINE_PCI_MSI 4

#define PLF_PLIC_IRQ_CFG                                \
    {PLF_INTLINE_UART0, PLIC_SRC_MODE_LEVEL_HIGH},      \
    {PLF_INTLINE_ETH0_RX, PLIC_SRC_MODE_LEVEL_HIGH},    \
    {PLF_INTLINE_ETH0_TX, PLIC_SRC_MODE_LEVEL_HIGH},    \
    {PLF_INTLINE_PCI_MSI, PLIC_SRC_MODE_LEVEL_HIGH},

#endif // PLATFORM_VCU118_SCR7_H
