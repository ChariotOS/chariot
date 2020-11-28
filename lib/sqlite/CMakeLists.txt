file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

set(SOURCES
	# src/shell.c
	src/sqlite3.c
	src/wrapper.cpp
)

add_definitions(
	-DSQLITE_THREADSAFE=0
	-DNO_GETTOD
	-DSQLITE_OMIT_LOAD_EXTENSION
	-DHAVE_USLEEP
)

# chariot_lib(libsqlite sqlite)
