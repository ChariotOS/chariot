file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_bin(wasm)

target_link_libraries(wasm PUBLIC libui libscm libwasm)
