
include_directories(../usr.include)


add_library(crt0 STATIC crt0.asm)
add_custom_command(
    TARGET crt0
    COMMAND install -D $<TARGET_OBJECTS:crt0> ${CMAKE_INSTALL_PREFIX}/usr/lib/crt0.o
)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	src/*.cpp
	src/*.c
	src/*.asm
)


chariot_libc(libc c)

target_link_libraries(libc crt0 "${CMAKE_CURRENT_LIST_DIR}/libgcc.a")
