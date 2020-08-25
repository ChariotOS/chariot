
set(SOURCES
		src/dynamic_cast.cc
		src/exception.cc
		src/guard.cc
		src/stdexcept.cc
		src/typeinfo.cc
		src/memory.cc
		src/auxhelper.cc
		src/libelftc_dem_gnu3.c
  )



set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fdiagnostics-color=always")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

chariot_libc(libcxxrt cxxrt)
target_link_libraries(libcxxrt libc)



# add_definitions(-D_GNU_SOURCE)
# add_library(cxxrt-static STATIC ${CXXRT_SOURCES})
# add_library(cxxrt-shared SHARED ${CXXRT_SOURCES})
# target_link_libraries(cxxrt-shared ${CMAKE_DL_LIBS})
# target_link_libraries(cxxrt-static ${CMAKE_DL_LIBS})
# set_target_properties(cxxrt-static cxxrt-shared PROPERTIES OUTPUT_NAME "cxxrt")



