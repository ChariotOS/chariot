file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_bin(game_of_life)
target_link_libraries(game_of_life PUBLIC libui)
