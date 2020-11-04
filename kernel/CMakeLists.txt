include(CMakeParseArguments)


# zero out the C_FLAGS
# set(CMAKE_C_FLAGS "-target x86_64-chariot-elf -fPIE -Wc99-designator -Wc++20-designator -Wdeprecated-volatile ")

set(CMAKE_C_FLAGS "")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__ARCH_${ARCH}__ -mno-red-zone -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__ARCH_${ARCH}__ -mno-red-zone -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -mtls-direct-seg-refs -fno-pie -Wno-sign-compare")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -mcmodel=large -Wall -fno-common")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-initializer-override -Wstrict-overflow=5")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -mno-sse")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-address-of-packed-member -Wno-strict-overflow -DKERNEL ")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -fno-rtti -fno-exceptions -Wno-literal-suffix")


set(CMAKE_ASM_NASM_FLAGS "-f elf64 -w-zext-reloc -D __ARCH_${ARCH}__")




add_definitions(-D FBWIDTH=1280)
add_definitions(-D FBHEIGHT=1024)
add_definitions(-D__ARCH_${ARCH}__)
add_definitions(-DKERNEL)


file(GLOB_RECURSE KERNEL_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	*.cpp
	*.c
	*.asm
)

add_link_options(LINKER:-T ${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld -nostdlib -z max-page-size=0x1000)

set(LDFLAGS -T ${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld -m elf_x86_64 -z max-page-size=0x1000)

include_directories(
	# for core kernel interfaces
	../usr.include/chariot
	# for lwip
	../net/lwip/src/include
	)

add_executable(chariot ${KERNEL_SOURCES})
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
		"SOURCES;GLOB;ARCH;AUTOGLOB" # multi value arguments

		${ARGN} # arguments of the function to parse, here we take the all original ones
	)

	if (TARGET_INTEL)
		set(TARGET_ARCH ${TARGET_ARCH} x86 x86_64 amd64)
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

	if (TARGET_CORE)
		message(STATUS "${TARGET_NAME} is a core module")
	endif()

	if (NOT TARGET_ARCH)
		set(TARGET_ARCH ${ARCH})
	endif()

	# we only want modules that are supported on this arch
	if (NOT ARCH IN_LIST TARGET_ARCH)
		message(STATUS "[arch] ${TARGET_NAME} is not valid for this arch (wants ${TARGET_ARCH}, has ${ARCH})")
		return()
	endif()

	add_library(${TARGET_NAME} STATIC ${TARGET_SOURCES})
	install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION mod)
	set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${TARGET_NAME}.klib")

	# add the library to the kernel's dependencies
	target_link_libraries(chariot -Wl,--whole-archive ${TARGET_NAME})

endfunction()


add_rooted_subdir(arch/x86)
add_rooted_subdir(fs)
add_rooted_subdir(net)
add_rooted_subdir(drivers)

