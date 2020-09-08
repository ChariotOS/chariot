file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_bin(ct)


target_link_libraries(ct libui
	# libosmesa
	"${CMAKE_CURRENT_SOURCE_DIR}/../../usr.lib/mesa/libOSMesa.a"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../usr.lib/mesa/libGLU.a"
	)

# target_link_libraries(ct libui libosmesa)
