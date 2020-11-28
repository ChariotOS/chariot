


set(SOURCES
	# core files
	src/cutils.c
	src/libbf.c
	src/libregexp.c
	src/libunicode.c
	src/quickjs.c
	# src/quickjs-libc.c
)


# file(STRINGS src/VERSION version)
# set(quickjs_def CONFIG_VERSION="${version}" _GNU_SOURCE CONFIG_BIGNUM)
# set(CMAKE_CFLAGS "${CMAKE_CFLAGS} -Wno-array-bounds -Wno-maybe-uninitialized ")
# include_directories(src)
# chariot_lib(libjs js)
# target_link_libraries(libjs PUBLIC libm)
# target_compile_definitions(libjs PRIVATE ${quickjs_def} )
