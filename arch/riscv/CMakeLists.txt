
include_directories(include)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
	*.S
)


add_kernel_module(
	CORE
	RISCV64
	NAME
		arch_riscv64
	SOURCES ${SOURCES}
)

