add_kernel_module(
	NAME virtio
	SOURCES disk.cpp
	NEEDS VIRTIO
)
