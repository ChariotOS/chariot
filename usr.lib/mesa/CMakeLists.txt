set(SOURCES main.c)

chariot_lib(libosmesa osmesa)

target_link_libraries(libosmesa
	"${CMAKE_CURRENT_LIST_DIR}/libOSMesa.a"
	"${CMAKE_CURRENT_LIST_DIR}/libGLU.a"
)
