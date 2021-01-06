
include_directories(../include /usr/include/c++/10.2.0/)


if(CONFIG_X86)
	set(CRT0_SRC "crt0/crt0.x86.asm")
endif()


if(CONFIG_RISCV)
	set(CRT0_SRC "crt0/crt0.riscv.S")
endif()

add_library(crt0 STATIC ${CRT0_SRC})
add_custom_command(
    TARGET crt0
    COMMAND install -D $<TARGET_OBJECTS:crt0> ${CMAKE_INSTALL_PREFIX}/usr/lib/crt0.o
)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	src/*.cpp
	src/*.c
	# the core of libc doesn't accept assembly files
)


file(GLOB_RECURSE ARCH_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	arch/${CONFIG_ARCH_DIR}/*.cpp
	arch/${CONFIG_ARCH_DIR}/*.c
	arch/${CONFIG_ARCH_DIR}/*.S
	arch/${CONFIG_ARCH_DIR}/*.asm
)

set(SOURCES ${SOURCES} ${ARCH_SOURCES})

chariot_libc(libc c)


# ask the compiler where libgcc is
execute_process(
	COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
	OUTPUT_VARIABLE LIBGCC_LOCATION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_link_libraries(libc crt0
	"-lgcc"
	# "${CMAKE_CURRENT_LIST_DIR}/libc++.a"
)


file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	cxxrt/dynamic_cast.cc
	cxxrt/exception.cc
	cxxrt/guard.cc
	cxxrt/stdexcept.cc
	cxxrt/typeinfo.cc
	cxxrt/memory.cc
	cxxrt/auxhelper.cc
	cxxrt/libelftc_dem_gnu3.c
)


chariot_libc(libstdc++ stdc++)


set(SOURCES dumb_gloss.c)
chariot_libc(libgloss gloss)

