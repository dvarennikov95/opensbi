platform-objs-y += platform.o

PLATFORM_RISCV_XLEN = 64
PLATFORM_RISCV_ABI = lp64
# For initial boot its enough to use "ima" instruction set 
# According to provided syntacore spec V64IMAFDC is supported
PLATFORM_RISCV_ISA = rv64ima
PLATFORM_RISCV_CODE_MODEL = medany
# fw start boot from SRAM 
FW_TEXT_START=0xA80000
# relocation mechanism needs to be debuged further , skip for now.
FW_PIC=n
FW_PAYLOAD=y
ifeq ($(PLATFORM_RISCV_XLEN), 32)
  # This needs to be 4MB aligned for 32-bit system
  FW_PAYLOAD_OFFSET=0x400000
else
  # This needs to be 2MB aligned for 64-bit system
  FW_PAYLOAD_OFFSET=0x200000
endif
# FW_PAYLOAD_PATH=/space2/users/ex-danielv/nextutils/RelWithDebInfo/nextrisc/bin/scr7_boot_test.bin
# FW_PAYLOAD_ALIGN=0x1000
FW_PAYLOAD_PATH=/space2/users/ex-danielv/u-boot/u-boot.bin 