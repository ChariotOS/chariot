cmake_minimum_required(VERSION 3.16)
project(ChariotOS C CXX ASM)
enable_language(ASM_NASM)


include(ExternalProject)
include(config.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_INCLUDE_HEADERS_IN_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_MESSAGE NEVER)
# set(CMAKE_VERBOSE_MAKEFILE on)'

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




set(TOOLCHAIN_PATH ${CMAKE_SOURCE_DIR}/toolchain/local/bin)
set(TOOLCHAIN_PREFIX ${TOOLCHAIN_PATH}/x86_64-elf-chariot-)




# set(POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

# set(CMAKE_C_COMPILER clang)
# set(CMAKE_CXX_COMPILER clang++)
# set(CMAKE_C_FLAGS "-target x86_64-chariot-elf -fPIE ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding ")

set(CMAKE_ASM_COMPILER nasm)
set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}strip)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)




set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR}/root)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/root)
# set(CMAKE_SYSROOT ${PROJECT_SOURCE_DIR}/base)

unset(CMAKE_SYSROOT)
set(CHARIOT_SYSROOT ${CMAKE_BINARY_DIR}/root)
add_link_options(--sysroot ${CHARIOT_SYSROOT})



set(EXECUTABLE_OUTPUT_PATH "${CHARIOT_SYSROOT}/bin" CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH "${CHARIOT_SYSROOT}/lib" CACHE PATH "Build directory" FORCE)


if(NOT DEFINED ARCH)
set(ARCH x86)
endif()



set(LDFLAGS -m elf_x86_64 -z max-page-size=0x1000)
set(CMAKE_ASM_NASM_FLAGS "-f elf64 -w-zext-reloc -D __ARCH_${ARCH}__")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__ARCH_${ARCH}__")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSERLAND")

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finstrument-functions ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -mmmx -fno-omit-frame-pointer -msse -msse2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-sign-compare")


set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -nostdinc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -fdiagnostics-color=always")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")



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
					target_link_libraries(${target_name} PUBLIC libstdc++ libck)
				endif()
				break()
			endif()
		endforeach(src)

endfunction()




function(add_res target_name file)
	set(output "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-${file}")

	add_custom_command(
		OUTPUT "${output}.res.o"
		DEPENDS ${file}
		COMMAND
		"${CMAKE_SOURCE_DIR}/tools/resource_generator.sh" "${CMAKE_CURRENT_SOURCE_DIR}/${file}" "${output}" "${file}"
		)

	target_sources(${target_name} PRIVATE "${output}.res.o")

endfunction()

function(chariot_bin target_name)

    add_executable(${target_name} ${SOURCES})
    install(TARGETS ${target_name} RUNTIME DESTINATION bin)
		target_link_libraries(${target_name} PUBLIC crt0 libc libm)

		foreach(src ${SOURCES})
			if (${src} MATCHES "cpp$")
				# message(STATUS "${target_name} is a c++ project")
				# only link in libck if it is a c++ project
				target_link_libraries(${target_name} PUBLIC libck)
				break()
			endif()
		endforeach(src)

endfunction()



function(chariot_external name archive_path)
	add_library(${name} STATIC IMPORTED GLOBAL)
	set_property(TARGET ${name} PROPERTY IMPORTED_LOCATION ${archive_path})
endfunction()



add_subdirectory(kernel)

add_subdirectory(libc)
add_subdirectory(usr.lib)
add_subdirectory(usr.bin)




chariot_external(libgl ${CMAKE_SOURCE_DIR}/ports/prebuilt/libOSMesa.a)
chariot_external(libglu ${CMAKE_SOURCE_DIR}/ports/prebuilt/libGLU.a)
chariot_external(libfreetype ${CMAKE_SOURCE_DIR}/ports/prebuilt/libfreetype.a)
