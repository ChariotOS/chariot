cmake_minimum_required(VERSION 3.13)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

project(ChariotOS C CXX ASM)

# The path to the toolchain we build.
# TODO: as it stands, this isn't different from the plain x86_64-elf-* toolchain you can just download
#       so I should change to that instead to save a step when building for the first time
set(TOOLCHAIN_PATH ${CMAKE_SOURCE_DIR}/toolchain/local/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_MESSAGE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_CROSS_COMPILING TRUE)

foreach(lang ASM C CXX OBJC OBJCXX)
	# unset(CMAKE_${lang}_OSX_COMPATIBILITY_VERSION_FLAG)
	#unset(CMAKE_${lang}_OSX_CURRENT_VERSION_FLAG)
	# unset(CMAKE_${lang}_LINK_FLAGS)
 	# unset(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS)
 	# unset(CMAKE_SHARED_MODULE_CREATE_${lang}_FLAGS)
 	# unset(CMAKE_SHARED_MODULE_LOADER_${lang}_FLAG )
 	# unset(CMAKE_${lang}_OSX_DEPLOYMENT_TARGET_FLAG)
 	# unset(CMAKE_${lang}_SYSROOT_FLAG)
 	# unset(CMAKE_SHARED_LIBRARY_SONAME_${lang}_FLAG)
endforeach()


set(CMAKE_OSX_DEPLOYMENT_TARGET "")

include(ExternalProject)
include(config.cmake)



if(CONFIG_X86)
	set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/x86_64-elf-)
	set(CMAKE_SYSTEM_PROCESSOR x86_64)
endif()


if(CONFIG_ARM64)
	set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/aarch64-elf-)
	set(CMAKE_SYSTEM_PROCESSOR aarch64)
endif()


if (CONFIG_RISCV)
	if(CONFIG_64BIT)
		set(RISCV_PREFIX "riscv64")
		set(RISCV_BITS "64")
	else()
		set(RISCV_PREFIX "riscv32")
		set(RISCV_BITS "32")
	endif()

	set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/${RISCV_PREFIX}-elf-)

	set(CMAKE_SYSTEM_PROCESSOR ${RISCV_PREFIX})
endif()

if(CONFIG_OVERRIDE_TOOLCHAIN)
	set(TOOLCHAIN_PREFIX "${CONFIG_TOOLCHAIN_PREFIX}")
endif()


if (CONFIG_CLANG)
	set(CMAKE_C_COMPILER clang)
	set(CMAKE_CXX_COMPILER clang++)
	set(CMAKE_ASM_COMPILER clang)
else()
	set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
	set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
	set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
endif()






set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}strip)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)



set(COMPILER_SPECIFIC_FLAGS "-Werror -Wformat=2 -Wformat-security")
if (CONFIG_CLANG)
	set(COMPILER_SPECIFIC_FLAGS "${COMPILER_SPECIFIC_FLAGS} -Wno-initializer-overrides -Wno-c99-designator ")
else()
	set(COMPILER_SPECIFIC_FLAGS "${COMPILER_SPECIFIC_FLAGS} -Wno-initializer-override ")
endif()



get_directory_property( DirDefs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMPILE_DEFINITIONS)

set(COMPILE_DEFS "")
foreach( d ${DirDefs} )
	set(COMPILE_DEFS ${COMPILE_DEFS} "-D ${d}")
endforeach()

string(JOIN " " COMPILE_DEFS_STR ${COMPILE_DEFS})

# Try to use ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()



if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
  message(FATAL
    ": Don't use CMAKE_BUILD_TYPE when building chariot.\n"
    "The default build type is optimized with debug info and asserts enabled,\n"
    "and that's all there is.")
endif()



include(arch/${CONFIG_ARCH_DIR}/config.cmake)



# "compile" the linker scripts
set(KERN_LINKER_SCRIPT "${CMAKE_BINARY_DIR}/kernel.ld")
set(USER_LINKER_SCRIPT "${CMAKE_BINARY_DIR}/user.ld")

execute_process(COMMAND ${TOOLCHAIN_PREFIX}cpp -P ${ARCH_KERN_LINKER_SCRIPT} -o ${KERN_LINKER_SCRIPT} ${COMPILE_DEFS})
execute_process(COMMAND ${TOOLCHAIN_PREFIX}cpp -P ${ARCH_USER_LINKER_SCRIPT} -o ${USER_LINKER_SCRIPT} ${COMPILE_DEFS})

set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR}/root)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/root)
# set(CMAKE_SYSROOT ${PROJECT_SOURCE_DIR}/base)

unset(CMAKE_SYSROOT)
set(CHARIOT_SYSROOT ${CMAKE_BINARY_DIR}/root)
add_link_options(--sysroot ${CHARIOT_SYSROOT})



set(EXECUTABLE_OUTPUT_PATH "${CHARIOT_SYSROOT}/bin" CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH "${CHARIOT_SYSROOT}/lib" CACHE PATH "Build directory" FORCE)

# warnings, etc.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fdiagnostics-color=always ${COMPILER_SPECIFIC_FLAGS} ")
if(CONFIG_DEBUG_SYMBOLS)
	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb ")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CONFIG_ADDITIONAL_USER_FLAGS}")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${ARCH_USER_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -nostdinc++ -std=c++20 ${ARCH_USER_CXX_FLAGS}")



function(chariot_libc target_name fs_name)
    add_library(${target_name} ${SOURCES})
    set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${fs_name})
		install(TARGETS ${target_name} ARCHIVE DESTINATION lib)
		target_link_directories(${target_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endfunction()




# A library is just a normal library. They cannot have resources
function(chariot_lib target_name fs_name)
		add_library(${target_name} STATIC ${SOURCES} ${GENERATED_SOURCES})
		install(TARGETS ${target_name} RUNTIME DESTINATION lib)

		target_link_libraries(${target_name} PUBLIC libc)
		set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${fs_name})

		foreach(src ${SOURCES})
			if (${src} MATCHES "cpp$")
				if (NOT ${target_name} STREQUAL "libck")
					# message(STATUS "${target_name} is a c++ project")
					# only link in libck if it is a c++ project
					# target_link_libraries(${target_name} PUBLIC libstdc++ libck libcpp)
					target_link_libraries(${target_name} PUBLIC libck libcpp)
				endif()
				break()
			endif()
		endforeach(src)

endfunction()



function(chariot_bin target_name)
    add_executable(${target_name} ${SOURCES})
    install(TARGETS ${target_name} RUNTIME DESTINATION bin)
		target_link_libraries(${target_name} PUBLIC crt0 libc libm)

		foreach(src ${SOURCES})
			if (${src} MATCHES "cpp$")
				# message(STATUS "${target_name} is a c++ project")
				# only link in libck if it is a c++ project
				target_link_libraries(${target_name} PUBLIC libck libcpp)
				break()
			endif()
		endforeach(src)

endfunction()


function(add_src)
	cmake_parse_arguments(
		SRC           # prefix of output variables
		""            # Boolean arguments
		""            # Single value arguments
		"FILES;NEEDS" # multi value arguments

		${ARGN} # arguments of the function to parse, here we take the all original ones
	)

message(STATUS ${CMAKE_CURRENT_LIST_DIR}/${SRC_FILES})
	foreach(need ${SRC_NEEDS})
		# we only want modules that are supported on this arch
		if (NOT need IN_LIST CONFIG_ENABLED_OPTIONS)
			message(STATUS "need ${need} not satisfied")
			return()
		endif()
	endforeach(need)

	set(SOURCES ${SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILES} PARENT_SCOPE)
endfunction()




function(add_res target_name file symbol)

	set(output "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-${file}")

	add_custom_command(
		OUTPUT "${output}.res.o"
		DEPENDS ${file}
		COMMAND
		"${CMAKE_SOURCE_DIR}/tools/resource_generator.sh" "${CMAKE_CURRENT_SOURCE_DIR}/${file}" "${output}" "${file}" "${symbol}"
		)

	target_sources(${target_name} PRIVATE "${output}.res.o")

endfunction()




function(chariot_external name archive_path)
	add_library(${name} STATIC IMPORTED GLOBAL)
	set_property(TARGET ${name} PROPERTY IMPORTED_LOCATION ${archive_path})
endfunction()



include_directories(kernel/include)

add_subdirectory(kernel)


include_directories(
	# C++ stuff
	toolchain/src/gcc-${CONFIG_GCC_VERSION}/libstdc++-v3/include/
	toolchain/src/gcc-${CONFIG_GCC_VERSION}/libstdc++-v3/include/c
	toolchain/src/gcc-${CONFIG_GCC_VERSION}/libstdc++-v3/include/c_std
	toolchain/src/gcc-${CONFIG_GCC_VERSION}/libstdc++-v3/include/c_global
	toolchain/src/gcc-${CONFIG_GCC_VERSION}/libstdc++-v3/include/std
	toolchain/src/gcc-${CONFIG_GCC_VERSION}/libstdc++-v3/libsupc++

	include
	libc/include
	kernel/include
)


if (CONFIG_ENABLE_USERSPACE)

	MACRO(SUBDIRLIST result curdir)
		FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
		SET(dirlist "")
		FOREACH(child ${children})
			IF(IS_DIRECTORY ${curdir}/${child})
				LIST(APPEND dirlist ${curdir}/${child})
			ENDIF()
		ENDFOREACH()
		SET(${result} ${dirlist})
	ENDMACRO()


	SUBDIRLIST(USER_BINS ${CMAKE_CURRENT_SOURCE_DIR}/bin)
	SUBDIRLIST(USER_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/lib)


	add_link_options(LINKER:-T ${USER_LINKER_SCRIPT} -z max-page-size=0x1000)

	FOREACH(subdir ${USER_BINS} ${USER_LIBS})
		include_directories(${subdir}/include)
	ENDFOREACH()


	FOREACH(subdir ${USER_BINS} ${USER_LIBS})
		add_subdirectory(${subdir})
	ENDFOREACH()

	add_subdirectory(libc)
	chariot_external(libcpp ${CMAKE_SOURCE_DIR}/toolchain/build/${CMAKE_SYSTEM_PROCESSOR}/gcc/libcpp/libcpp.a)
	chariot_external(libfreetype ${CMAKE_SOURCE_DIR}/ports/out/${CMAKE_SYSTEM_PROCESSOR}/lib/libfreetype.a)


endif(CONFIG_ENABLE_USERSPACE)

# everyone needs this.
chariot_external(libgcc ${CMAKE_SOURCE_DIR}/toolchain/build/${CMAKE_SYSTEM_PROCESSOR}/gcc/gcc/libgcc.a)
