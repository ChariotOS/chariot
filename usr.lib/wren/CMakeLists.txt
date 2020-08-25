


set(SOURCES
	# core files
	src/vm/wren_compiler.c
	src/vm/wren_core.c
	src/vm/wren_debug.c
	src/vm/wren_primitive.c
	src/vm/wren_utils.c
	src/vm/wren_value.c
	src/vm/wren_vm.c
	src/vm/wren_bind.cpp

	# optional
	src/optional/wren_opt_meta.c
	src/optional/wren_opt_random.c
)

include_directories(src/vm src/optional src/include)

# chariot_lib(libwren wren)
# target_link_libraries(libwren libm)
