cmake_minimum_required (VERSION 2.6)
project (mobo)
set (CMAKE_CXX_STANDARD 17)


add_compile_options(
  "-g"
)

include_directories ("${PROJECT_SOURCE_DIR}/include")
add_executable(mobo
	src/machine.cpp
	src/kvmdriver.cpp
	src/main.cpp
	src/dev_mgr.cpp

	src/devices/debug.cpp
	)

target_link_libraries(mobo -lcapstone)
