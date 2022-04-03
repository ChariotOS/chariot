
if(CONFIG_64BIT)
	set(RISCV_MARCH "rv64gc")
	set(RISCV_MABI  "lp64d")
else()
	set(RISCV_MARCH "rv32gc")
	set(RISCV_MABI  "ilp32d")
endif()

# universal
set(LDFLAGS -m elf_riscv64 -z max-page-size=0x1000 -nostdlib -pie)

# set(CMAKE_ASM_FLAGS "${RISCV_FLAGS} -mno-relax ")
set(CMAKE_ASM_FLAGS "${RISCV_FLAGS} ")


# TODO: riscv userspace flags
set(ARCH_USER_C_FLAGS "-march=${RISCV_MARCH} -mabi=${RISCV_MABI} -fno-omit-frame-pointer")
set(ARCH_USER_CXX_FLAGS "")

# Kernelspace Flags
set(ARCH_KERN_C_FLAGS "-march=${RISCV_MARCH} -mabi=${RISCV_MABI} -mcmodel=medany -fno-stack-protector -fno-omit-frame-pointer")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -fpic")
set(ARCH_KERN_CXX_FLAGS "")



set(ARCH_KERN_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/arch/riscv/ld/qemu-virt64-kernel.ld")
set(ARCH_USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/arch/riscv/user.ld")
