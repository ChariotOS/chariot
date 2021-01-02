
if(CONFIG_64BIT)
	set(RISCV_MARCH "rv64gc")
	set(RISCV_MABI  "lp64")
else()
	set(RISCV_MARCH "rv32gc")
	set(RISCV_MABI  "ilp32")
endif()

# core gcc flags to select the riscv arch correctly
set(RISCV_FLAGS "-march=${RISCV_MARCH} -mabi=${RISCV_MABI} -mcmodel=medany")

message(STATUS "${RISCV_FLAGS}")

# universal
set(LDFLAGS -m elf_riscv64 -z max-page-size=0x1000 --no-relax)

set(CMAKE_ASM_FLAGS "${RISCV_FLAGS} -mno-relax ")


# TODO: riscv userspace flags
set(ARCH_USER_C_FLAGS "")
set(ARCH_USER_CXX_FLAGS "")

# Kernelspace Flags
set(ARCH_KERN_C_FLAGS "-mno-relax ${RISCV_FLAGS} -fno-stack-protector")
set(ARCH_KERN_CXX_FLAGS "")



set(ARCH_KERN_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/arch/riscv/ld/qemu-virt64-kernel.ld")
set(ARCH_USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/arch/riscv/user.ld")
