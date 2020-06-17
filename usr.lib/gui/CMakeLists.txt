file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

chariot_lib(libgui gui)

target_link_libraries(libgui liblumen libgfx)

