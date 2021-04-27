file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)



add_library(shared SHARED shared_test.c)
