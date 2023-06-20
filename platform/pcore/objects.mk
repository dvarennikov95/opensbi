platform-objs-y += platform.o
platform-objs-y += mmu.o

PLATFORM_RISCV_XLEN = 64
PLATFORM_RISCV_ABI = lp64
# For initial boot its enough to use "ima" instruction set 
# According to provided syntacore spec V64IMAFDC is supported
PLATFORM_RISCV_ISA = rv64ima
PLATFORM_RISCV_CODE_MODEL = medany
# fw start boot from SRAM 
# FW_TEXT_START=0xFFFF8FFFF8000000
# fw start boot from HBM
FW_TEXT_START=0xA80000 # for sram 0xFFFF8FFFF8000000
# relocation mechanism needs to be debuged further , skip for now.
FW_PIC=n
FW_PAYLOAD=y
# FW_PAYLOAD_ALIGN=0x100000
ifeq ($(PLATFORM_RISCV_XLEN), 32)
  # This needs to be 4MB aligned for 32-bit system
  FW_PAYLOAD_OFFSET=0x400000
else
  # This needs to be 2MB aligned for 64-bit system
  FW_PAYLOAD_OFFSET=0x200000 # for sram 0x100000
endif
# path to uboot binary must be included when you build 
# make CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=pcore FW_PAYLOAD_PATH=/space2/users/ex-danielv/u-boot/u-boot.bin
# FW_PAYLOAD_PATH=
