
add_kernel_module(
	NAME ext2fs
	NEEDS EXT2
	SOURCES
		ext2/ext2.cpp
		ext2/inode.cpp
)

add_kernel_module(
	NAME tmpfs
	SOURCES
		tmpfs/tmpfs.cpp
)


add_kernel_module(
	NAME procfs
	NEEDS PROCFS
	SOURCES
		procfs/superblock.cpp
		procfs/file.cpp
)

add_kernel_module(NAME console SOURCES console.cpp)
add_kernel_module(NAME tty SOURCES tty.cpp)
add_kernel_module(NAME pty SOURCES pty.cpp)
