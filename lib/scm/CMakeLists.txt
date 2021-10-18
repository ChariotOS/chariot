
set(CMAKE_CXX_FLAGS
	"${CMAKE_CXX_FLAGS} -Wno-c++11-compat-deprecated-writable-strings -Wno-write-strings")

set(SOURCES
	src/scheme.cpp
	src/bind.cpp
)

# file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)
chariot_lib(libscm scm)
