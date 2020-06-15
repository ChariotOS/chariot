cmake_minimum_required(VERSION 3.16)
project(ChariotOS C CXX ASM)
enable_language(ASM_NASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
  message(FATAL
    ": Don't use CMAKE_BUILD_TYPE when building chariot.\n"
    "The default build type is optimized with debug info and asserts enabled,\n"
    "and that's all there is.")
endif()


set(TOOLCHAIN_PATH ${CMAKE_SOURCE_DIR}/toolchain/local/bin)
set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/x86_64-elf-chariot-)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER nasm)
set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}strip)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)


set(ARCH x86_64)
set(LDFLAGS -m elf_x86_64 -z max-page-size=0x1000)

set(CMAKE_ASM_NASM_FLAGS "-f elf64 -w-zext-reloc -D __ARCH_${ARCH}__")



unset(CMAKE_SYSROOT)
set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR}/base)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/base)
set(CMAKE_INSTALL_DATAROOTDIR ${CMAKE_BINARY_DIR}/base/res)


add_link_options(--sysroot ${CMAKE_BINARY_DIR}/base)

add_subdirectory(kernel)
