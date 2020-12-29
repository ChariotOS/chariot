file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_bin(mpeg)
target_link_libraries(mpeg PUBLIC libui)
