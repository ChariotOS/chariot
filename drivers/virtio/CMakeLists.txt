add_kernel_module(
	NAME virtio
	AUTOGLOB ./
	NEEDS CONFIG_VIRTIO
)