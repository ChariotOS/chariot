include(CMakeParseArguments)


set(CMAKE_C_FLAGS "-DKERNEL")
if(CONFIG_DEBUG_SYMBOLS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

if(CONFIG_OPTIMIZED)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${ARCH_KERN_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${ARCH_KERN_CXX_FLAGS}")


file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
	*.asm
)

set(SOURCES ${SOURCES} ${ARCH_KERN_SOURCES})


add_link_options(LINKER:-T "${ARCH_KERN_LINKER_SCRIPT}" -nostdlib -z max-page-size=0x1000)


include_directories(
	# for core kernel interfaces
	../include/chariot
	# for lwip
	../net/lwip/src/include
	)

add_executable(chariot ${SOURCES})
set_target_properties(chariot PROPERTIES OUTPUT_NAME "chariot.elf")
install(TARGETS chariot RUNTIME DESTINATION boot)


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
			# message(STATUS "${TARGET_NAME} needs ${need} but it was not enabled")
			return()
		endif()
	endforeach(need)

	add_library(${TARGET_NAME} STATIC ${TARGET_SOURCES})
	install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION mod)
	set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${TARGET_NAME}.klib")

	# add the library to the kernel's dependencies
	target_link_libraries(chariot -Wl,--whole-archive ${TARGET_NAME})

endfunction()


add_rooted_subdir(arch/${CHARIOT_ARCH})


if(CONFIG_X86)
	add_rooted_subdir(fs)
	add_rooted_subdir(net)
	add_rooted_subdir(drivers)
endif()

