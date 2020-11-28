

set(curdir ${CMAKE_CURRENT_SOURCE_DIR}/)
FILE(GLOB CORE_BINS RELATIVE ${curdir} ${curdir}/*.cpp ${curdir}/*.c)
FOREACH(BIN ${CORE_BINS})
	get_filename_component(NAME ${BIN} NAME_WE)
	set(SOURCES ${curdir}${BIN})
	chariot_bin(${NAME})
ENDFOREACH()
