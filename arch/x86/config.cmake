# use nasm on x86
# set(CMAKE_ASM_COMPILER nasm)
enable_language(ASM_NASM)


# universal
set(LDFLAGS -m elf_x86_64 -z max-page-size=0x1000)

# unique arch flags for userspace
set(ARCH_USER_C_FLAGS "-mmmx -fno-omit-frame-pointer -msse -msse2 ")
set(ARCH_USER_CXX_FLAGS "")
set(CMAKE_ASM_NASM_FLAGS "-f elf64 -w-zext-reloc ${COMPILE_DEFS_STR}")


# Kernelspace Flags
set(ARCH_KERN_C_FLAGS "-mno-red-zone -fno-omit-frame-pointer")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -fno-stack-protector -mtls-direct-seg-refs")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -mno-sse -mcmodel=large")
set(ARCH_KERN_CXX_FLAGS "")



set(ARCH_KERN_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/arch/x86/kernel.ld")
set(ARCH_USER_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/arch/x86/user.ld")

