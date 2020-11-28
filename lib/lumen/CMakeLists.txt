file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_lib(liblumen lumen)


target_link_libraries(liblumen PUBLIC libgfx)
