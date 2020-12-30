include_directories(include)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
	*.S
)


message(STATUS ${SOURCES})


add_kernel_module(
	CORE
	RISCV64
	NAME
		arch_riscv64
	SOURCES
		${SOURCES}
)

# chariot_external(libsbi ${CMAKE_CURRENT_SOURCE_DIR}/opensbi/platform/virt/libsbi.a)
# chariot_external(libsbiutils ${CMAKE_CURRENT_SOURCE_DIR}/opensbi/platform/virt/libsbiutils.a)
# target_link_libraries(arch_riscv64 libsbiutils)
