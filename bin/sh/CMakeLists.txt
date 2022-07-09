file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

chariot_bin(sh)

target_link_libraries(sh PUBLIC libtreesitter)
