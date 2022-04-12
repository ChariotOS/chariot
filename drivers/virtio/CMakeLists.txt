add_kernel_module(
	NAME virtio
	SOURCES virtio.cpp disk.cpp # input.cpp
	NEEDS VIRTIO
)
