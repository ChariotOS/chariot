file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_bin(donut)


target_link_libraries(donut PUBLIC libui)
