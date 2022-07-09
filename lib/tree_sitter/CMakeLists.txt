file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

include_directories(src)
chariot_lib(libtreesitter treesitter)
