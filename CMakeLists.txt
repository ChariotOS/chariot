cmake_minimum_required(VERSION 3.16)
project(ChariotOS C CXX ASM)
enable_language(ASM_NASM)

# The path to the toolchain we build.
# TODO: as it stands, this isn't different from the plain x86_64-elf-* toolchain you can just download
#       so I should change to that instead to save a step when building for the first time
set(TOOLCHAIN_PATH ${CMAKE_SOURCE_DIR}/toolchain/local/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_MESSAGE NEVER)

include(ExternalProject)
include(config.cmake)


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


if(CONFIG_X86)
	set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/x86_64-elf-)

	set(CHARIOT_ARCH x86)
endif()


if(CONFIG_ARM64)
	set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/aarch64-elf-)

	set(CHARIOT_ARCH arm64)
endif()



set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}strip)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)

include(arch/${CHARIOT_ARCH}/config.cmake)


message(STATUS "${CMAKE_LINKER}")
# set(POSITION_INDEPENDENT_CODE TRUE)





set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR}/root)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/root)
# set(CMAKE_SYSROOT ${PROJECT_SOURCE_DIR}/base)

unset(CMAKE_SYSROOT)
set(CHARIOT_SYSROOT ${CMAKE_BINARY_DIR}/root)
add_link_options(--sysroot ${CHARIOT_SYSROOT})



set(EXECUTABLE_OUTPUT_PATH "${CHARIOT_SYSROOT}/bin" CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH "${CHARIOT_SYSROOT}/lib" CACHE PATH "Build directory" FORCE)



set(CMAKE_C_FLAGS "-DUSERLAND")
if(CONFIG_DEBUG_SYMBOLS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

if(CONFIG_OPTIMIZED)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${ARCH_USER_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${ARCH_USER_CXX_FLAGS}")



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
					target_link_libraries(${target_name} PUBLIC libstdc++ libck libcpp)
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



add_subdirectory(kernel)

add_subdirectory(libc)
add_subdirectory(lib)
add_subdirectory(bin)



chariot_external(libcpp ${CMAKE_SOURCE_DIR}/toolchain/build/gcc/build-x86_64-pc-linux-gnu/libcpp/libcpp.a)
chariot_external(libgl ${CMAKE_SOURCE_DIR}/ports/prebuilt/libOSMesa.a)
chariot_external(libglu ${CMAKE_SOURCE_DIR}/ports/prebuilt/libGLU.a)
chariot_external(libfreetype ${CMAKE_SOURCE_DIR}/ports/prebuilt/libfreetype.a)
