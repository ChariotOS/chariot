

include(ExternalProject)


set(MESA_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/mesa)
set(MESA_SRC ${MESA_ROOT}/src)
set(MESA_BIN ${MESA_ROOT}/bin)

set(MESA_INCLUDES ${MESA_BIN}/include)

file(MAKE_DIRECTORY ${MESA_INCLUDES})

message(STATUS ${MESA_BIN}/lib/libOSMesa.a)



# add_custom_target(mesa-git ALL
# COMMAND git clone https://github.com/nickwanninger/mesa-chariot.git ${MESA_ROOT}
# BYPRODUCTS ${MESA_ROOT}
# )



ExternalProject_Add(
    project_mesa

		GIT_REPOSITORY https://github.com/nickwanninger/mesa-chariot.git
		GIT_TAG master

		SOURCE_DIR        ${MESA_SRC}
		BINARY_DIR        ${MESA_SRC}

		UPDATE_COMMAND ""


		CONFIGURE_COMMAND cmake -E env PATH=${TOOLCHAIN_PATH}/:$ENV{PATH}
			./configure
						# --build x86_64-elf-chariot
						# --host x86_64-elf-chariot
						# --srcdir=${MESA_SRC}
						--prefix=${MESA_BIN}
						--with-osmesa-bits=8
						--with-driver=osmesa
						--disable-egl
						--disable-shared
						--without-x
						--disable-glw
						--disable-glut
						--disable-driglx-direct
						--disable-gallium
						"CFLAGS=-I${CMAKE_SOURCE_DIR}/usr.include -fno-stack-protector"
						"CXXFLAGS=-I${CMAKE_SOURCE_DIR}/usr.include -fno-stack-protector"

		BUILD_COMMAND cmake -E env "PATH=${TOOLCHAIN_PATH}/:$ENV{PATH}" make -j
    BUILD_BYPRODUCTS ${MESA_BIN}/lib/libOSMesa.a ${MESA_BIN}/lib/libGLU.a
)



add_library(osmesa STATIC IMPORTED GLOBAL)
set_property(TARGET osmesa PROPERTY IMPORTED_LOCATION ${MESA_BIN}/lib/libOSMesa.a)
add_dependencies(osmesa project_mesa)


add_library(glu STATIC IMPORTED GLOBAL)
set_property(TARGET glu PROPERTY IMPORTED_LOCATION ${MESA_BIN}/lib/libGLU.a)
add_dependencies(glu project_mesa)
