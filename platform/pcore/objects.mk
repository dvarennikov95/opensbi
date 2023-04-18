platform-objs-y += platform.o

PLATFORM_RISCV_XLEN = 64
PLATFORM_RISCV_ABI = lp64
# For initial boot its enough to use "ima" instruction set 
# According to provided syntacore spec V64IMAFDC is supported
PLATFORM_RISCV_ISA = rv64ima  
PLATFORM_RISCV_CODE_MODEL = medany
# fw start boot from SRAM 
# FW_TEXT_START=0xFFFF8FFFF8000000
# fw start boot from HBM
FW_TEXT_START=0xA80000
# relocation mechanism needs to be debuged further , skip for now.
FW_PIC=n
FW_PAYLOAD=y
FW_PAYLOAD_ALIGN=0x100000