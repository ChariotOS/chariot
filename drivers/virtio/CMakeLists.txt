add_kernel_module(
	NAME virtio
	SOURCES virtio.cpp disk.cpp gpu.cpp
	NEEDS VIRTIO
)
