cmake_minimum_required (VERSION 2.6)
project (mobo)
set (CMAKE_CXX_STANDARD 17)


add_compile_options(
  "-g"
)


include_directories ("${PROJECT_SOURCE_DIR}/include")


file( GLOB DEVICES src/devices/*.cpp )

add_executable(mobo
	src/machine.cpp
	src/kvm.cpp
	src/main.cpp
	src/dev_mgr.cpp
	${DEVICES}
	)

target_link_libraries(mobo -lcapstone)
