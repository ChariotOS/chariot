

# zero out the C_FLAGS
set(CMAKE_C_FLAGS "")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__ARCH_${ARCH}__ -mno-red-zone -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -mtls-direct-seg-refs -fno-pie -Wno-sign-compare")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -mcmodel=large -g -O3 -Wall -fno-common")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-initializer-override -Wstrict-overflow=5 -fno-tree-vectorize")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-address-of-packed-member -Wno-strict-overflow -DGIT_REVISION=\"git revision, idk\" -DKERNEL ")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -fno-rtti -fno-exceptions")


set(CMAKE_ASM_NASM_FLAGS "-f elf64 -w-zext-reloc -D __ARCH_${ARCH}__")


file(GLOB_RECURSE KERNEL_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
	*.asm

	../net/*.asm
	../net/*.cpp
	../net/*.c

	../arch/${ARCH}/*.asm
	../arch/${ARCH}/*.cpp
	../arch/${ARCH}/*.c

	../fs/*.asm
	../fs/*.cpp
	../fs/*.c

	../drivers/*.asm
	../drivers/*.cpp
	../drivers/*.c
	)

add_link_options(LINKER:-T ${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld -nostdlib -z max-page-size=0x1000)

set(LDFLAGS -T ${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld -m elf_x86_64 -z max-page-size=0x1000)

include_directories(../include)



set(SOURCES ${KERNEL_SOURCES})
add_executable(kernel ${SOURCES})
set_target_properties(kernel PROPERTIES OUTPUT_NAME "chariot.elf")
install(TARGETS kernel RUNTIME DESTINATION boot)
