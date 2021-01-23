file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)

chariot_lib(libgfx gfx)

target_link_libraries(libgfx PUBLIC libm)
# set_target_properties(libgfx PROPERTIES INTERFACE_LINK_LIBRARIES libfreetype)
target_link_libraries(libgfx INTERFACE libfreetype libcairo)
