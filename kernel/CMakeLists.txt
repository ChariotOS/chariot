include(CMakeParseArguments)


set(CMAKE_C_FLAGS "-DKERNEL -ffreestanding -Wall -fno-common")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-initializer-override -Wstrict-overflow=5")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused -Wno-address-of-packed-member -Wno-strict-overflow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-pie -Wno-sign-compare")
if(CONFIG_DEBUG_SYMBOLS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CONFIG_ADDITIONAL_KERNEL_FLAGS}")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${ARCH_KERN_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++20 -fno-rtti -Wno-invalid-offsetof -fno-exceptions -Wno-literal-suffix -fpermissive ${ARCH_KERN_CXX_FLAGS}")


# get the kernel sources
file(GLOB_RECURSE KERNEL_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
)

list(TRANSFORM TARGET_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

SET(KERNEL_SOURCES  "${KERNEL_SOURCES}" CACHE INTERNAL "KERNEL_SOURCES")




add_link_options(LINKER:-T "${KERN_LINKER_SCRIPT}" -nostdlib -z max-page-size=0x1000)


# for core kernel interfaces
include_directories(include/chariot)
include_directories(../include/chariot)
include_directories(../fs/ext4/include)

# LWIP interface
include_directories(../net/lwip/include)



set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}" CACHE PATH "kernel build" FORCE)



macro(add_rooted_subdir dir)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../${dir} ${CMAKE_CURRENT_BINARY_DIR}/kroot/${dir})
endmacro()




function(add_kernel_module)
	cmake_parse_arguments(
		TARGET # prefix of output variables
		"CORE;INTEL" # Boolean arguments
		"NAME;QUICK" # Single value arguments
		"SOURCES;GLOB;AUTOGLOB;NEEDS" # multi value arguments

		${ARGN} # arguments of the function to parse, here we take the all original ones
	)

	if (TARGET_INTEL)
		set(TARGET_NEEDS ${TARGET_NEEDS} X86)
	endif()


	if (TARGET_QUICK)
		set(TARGET_NAME ${TARGET_QUICK})
		set(TARGET_AUTOGLOB ${TARGET_QUICK})
	endif()


	# note: if it remains unparsed arguments, here, they can be found in variable TARGET_UNPARSED_ARGUMENTS
	if(NOT TARGET_NAME)
			message(FATAL_ERROR "You must provide a name")
	endif()




	if (TARGET_GLOB)
		file(GLOB_RECURSE GLOB_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${TARGET_GLOB})
		set(TARGET_SOURCES ${TARGET_SOURCES} ${GLOB_SOURCES})
	endif()

	if (TARGET_AUTOGLOB)
		set(globdir "${CMAKE_CURRENT_SOURCE_DIR}")
		file(GLOB_RECURSE GLOB_SOURCES RELATIVE "${globdir}"
			"${TARGET_AUTOGLOB}/*.cpp"
			"${TARGET_AUTOGLOB}/*.c"
			"${TARGET_AUTOGLOB}/*.asm"
			)
		set(TARGET_SOURCES ${TARGET_SOURCES} ${GLOB_SOURCES})
	endif()

	foreach(need ${TARGET_NEEDS})
		# we only want modules that are supported on this arch
		if (NOT need IN_LIST CONFIG_ENABLED_OPTIONS)
			# message(STATUS "- MODULE ${TARGET_NAME} needs ${need} but it was not enabled")
			return()
		endif()
	endforeach(need)


	list(TRANSFORM TARGET_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")



	# message(STATUS "+ MODULE ${TARGET_NAME} with ${TARGET_SOURCES}")

	# add_library(${TARGET_NAME} STATIC ${TARGET_SOURCES})
	# install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION mod)
	# set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${TARGET_NAME}.klib")

	# add the library to the kernel's dependencies
	# target_link_libraries(chariot -Wl,--whole-archive ${TARGET_NAME})

	# list(APPEND KERNEL_SOURCES ${TARGET_SOURCES}) 

	set(NEW_SOURCES ${KERNEL_SOURCES} ${TARGET_SOURCES})
	SET(KERNEL_SOURCES  "${NEW_SOURCES}" CACHE INTERNAL "KERNEL_SOURCES")
endfunction()


add_rooted_subdir(arch/${CONFIG_ARCH_DIR})

# target_link_libraries(chariot -Wl,--whole-archive ${CMAKE_SOURCE_DIR}/toolchain/build/${CMAKE_SYSTEM_PROCESSOR}/gcc/gcc/libgcc.a)

add_rooted_subdir(fs)
add_rooted_subdir(net)
add_rooted_subdir(drivers)


# recompile libck for the kernel, as it needs some special macros to work correctly
# add_kernel_module(
# 	NAME ck-kernel
# 	AUTOGLOB ../lib/ck/
# )



# add_executable(chariot ${KERNEL_SOURCES})
add_library(chariot STATIC ${KERNEL_SOURCES})
set_target_properties(chariot PROPERTIES OUTPUT_NAME "chariot")

install(TARGETS chariot RUNTIME DESTINATION bin)

# Link (without) an initrd. See `tools/initrd.sh` to see how an initrd is linked
add_executable(chariot_exe $<TARGET_OBJECTS:chariot>)
set_target_properties(chariot_exe PROPERTIES OUTPUT_NAME "chariot.elf")
target_link_libraries(chariot_exe libgcc)

