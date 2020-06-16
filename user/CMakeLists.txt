
# make sure the userspace programs and libraries use this 
add_link_options(LINKER:-T ${CMAKE_CURRENT_SOURCE_DIR}/user.ld -z max-page-size=0x1000 -nostdlib)

include_directories(include)


MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()


SUBDIRLIST(USER_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/lib)
FOREACH(subdir ${USER_LIBS})
	add_subdirectory(lib/${subdir})
ENDFOREACH()


SUBDIRLIST(USER_PROGS ${CMAKE_CURRENT_SOURCE_DIR}/bin)
FOREACH(subdir ${USER_PROGS})
	add_subdirectory(bin/${subdir})
ENDFOREACH()
