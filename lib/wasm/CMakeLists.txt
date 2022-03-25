
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dd_m3HasWASI -g3 ")
set(SOURCES
    "src/m3_api_libc.c"
    "src/m3_api_wasi.c"
		# "src/m3_api_uvwasi.c"
		# "src/m3_api_meta_wasi.c"
    "src/m3_api_tracer.c"
    "src/m3_bind.c"
    "src/m3_code.c"
    "src/m3_compile.c"
    "src/m3_core.c"
    "src/m3_env.c"
    "src/m3_exec.c"
    "src/m3_function.c"
    "src/m3_info.c"
    "src/m3_module.c"
    "src/m3_parse.c"
)

chariot_lib(libwasm wasm)
