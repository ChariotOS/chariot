
add_link_options(LINKER:-T ${CMAKE_CURRENT_SOURCE_DIR}/../user.ld -z max-page-size=0x1000 -nostdlib)
# make sure the userspace programs and libraries use this

include_directories(../usr.include ../usr.lib/c++/include)

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


SUBDIRLIST(USER_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/)
FOREACH(subdir ${USER_LIBS})
	add_subdirectory(${subdir})
ENDFOREACH()


# add_subdirectory(term)
# add_subdirectory(ct)
# add_subdirectory(init)
