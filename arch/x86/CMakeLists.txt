

include_directories(
	include/
)


add_kernel_module(
	CORE
	INTEL
	NAME
		arch_x86
	GLOB
		*.cpp
		*.c
		*.asm
		*.S
)

# message(STATUS "-m32 ${ARCH_KERN_BASE_C_FLAGS}")
set_source_files_properties(early.c PROPERTIES CMAKE_C_FLAGS "-m32 ${ARCH_KERN_BASE_C_FLAGS}")
