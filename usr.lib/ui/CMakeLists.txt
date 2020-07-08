file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

chariot_lib(libui ui)

target_link_libraries(libui liblumen libgfx)

