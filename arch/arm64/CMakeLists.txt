
include_directories(include)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
	*.S
)


add_kernel_module(
	NAME BCM28XX
	NEEDS BCM28XX
	AUTOGLOB platform/bcm28xx/
)


add_kernel_module(
	NAME QEMU_VIRT_ARM64
	NEEDS QEMU_VIRT_ARM64
	AUTOGLOB platform/qemu-virt-arm/
)

add_kernel_module(
	CORE
	ARM64
	NAME
		arch_arm64
	SOURCES ${SOURCES}
)

