# ExternalProject_Add(
# 	rust_bins
# 	DOWNLOAD_COMMAND ""
# 	CONFIGURE_COMMAND ""
# 	BUILD_COMMAND cargo build
# 	# COMMAND cargo build --release
# 	BINARY_DIR "${CMAKE_SOURCE_DIR}/bin/rs"
# 	INSTALL_COMMAND ""
# 	LOG_BUILD ON
# )
# 
# add_dependencies(rust_bins libc)
