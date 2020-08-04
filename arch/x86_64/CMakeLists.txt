file(GLOB_RECURSE ARCH_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

add_kernel_module(
	CORE # this module *must* be linked into the core binary
	ARCH x86_64 i386
	NAME arch_x86
	SOURCES
		${ARCH_SOURCES}
)


