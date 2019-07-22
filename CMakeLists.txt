cmake_minimum_required (VERSION 2.6)
project (mobo)
set (CMAKE_CXX_STANDARD 17)


# mobo requires LLVM 8
# find_package(LLVM 8 CONFIG)
# llvm_map_components_to_libnames(LLVM_LIBS all)
# message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
# message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# message(STATUS "LLVM_DEFINITIONS: ${LLVM_DEFINITIONS}")
# message(STATUS "LLVM_LIBS: ${LLVM_LIBS}")



# set(ASMJIT_STATIC TRUE)
# set(ASMJIT_RELEASE TRUE)
# add_subdirectory(dep/asmjit)



add_compile_options(
  "-g"
)


include_directories ("${PROJECT_SOURCE_DIR}/include")

file(GLOB DEVICES src/devices/*.cpp )

add_executable(mobo
	src/machine.cpp
	src/kvm.cpp
	src/main.cpp
	src/dev_mgr.cpp
	${DEVICES}
	)

target_include_directories(mobo PRIVATE ${LLVM_INCLUDE_DIRS})
#target_link_libraries(mobo ${LLVM_LIBS} ${CMAKE_DL_LIBS} -lcapstone)
target_link_libraries(mobo ${CMAKE_DL_LIBS} -lcapstone)
