

add_kernel_module(
	NAME ahci
	INTEL
	SOURCES
		ahci/ahci.cpp
		ahci/sata.cpp
)


add_kernel_module(
	NAME ata
	INTEL
	SOURCES
		ata/ata.cpp
)


add_kernel_module(
	NAME nvme
	INTEL
	AUTOGLOB nvme/
)

add_kernel_module(
	NAME char
	SOURCES char/generic.cpp
)

add_kernel_module(
	NAME e1000
	INTEL
	AUTOGLOB e1000/
)


add_kernel_module(
	NAME ps2_hid
	INTEL
	AUTOGLOB ps2/
)


add_kernel_module(
	NAME vga
	INTEL
	AUTOGLOB vga/
)


add_kernel_module(
	NAME RTL8139
	INTEL
	AUTOGLOB RTL8139/
)


add_kernel_module(
	NAME sb16
	INTEL
	AUTOGLOB sb16/
)


# add_kernel_module(NAME sb16)
# add_kernel_module(NAME virtio)
add_kernel_module(
	NAME vmware
	INTEL
	AUTOGLOB vmware/
)


add_kernel_module(
	NAME COM
	INTEL
	AUTOGLOB com/
)
