# use nasm on x86
# set(CMAKE_ASM_COMPILER nasm)
# enable_language(ASM_NASM)


# set(CMAKE_C_COMPILER clang)
# set(CMAKE_CXX_COMPILER clang++)

# universal
set(LDFLAGS -m elf_aarch64 -z max-page-size=0x1000)

# flags for C and C++ in userspace
set(ARCH_USER_C_FLAGS "-ffreestanding -fno-omit-frame-pointer -fdiagnostics-color=always ")
# unique flags to C++ in userspace
set(ARCH_USER_CXX_FLAGS "-nostdinc++ -std=c++20")

# Kernelspace Flags
set(ARCH_KERN_C_FLAGS "-nostdlib")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -fno-stack-protector -fno-pie -Wno-sign-compare")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -ffreestanding -Wall -fno-common")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -Wno-initializer-overrides -Wstrict-overflow=5")
set(ARCH_KERN_C_FLAGS "${ARCH_KERN_C_FLAGS} -Wno-unused -Wno-address-of-packed-member -Wno-strict-overflow -DKERNEL ")

set(ARCH_KERN_CXX_FLAGS "-std=c++20 -fno-rtti -fno-exceptions -Wno-reserved-user-defined-literal")



set(ARCH_KERN_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/arch/arm64/kernel.ld")
set(ARCH_USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/arch/arm64/user.ld")
