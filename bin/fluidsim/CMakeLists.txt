file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_bin(fluidsim)
target_link_libraries(fluidsim PUBLIC libui)
