

add_kernel_module(
	NAME ahci
	NEEDS AHCI X86
	SOURCES
		ahci/ahci.cpp
		ahci/sata.cpp
)


add_kernel_module(
	NAME ata
	NEEDS ATA X86
	SOURCES
		ata/ata.cpp
)


add_kernel_module(
	NAME nvme
	NEEDS NVME X86
	AUTOGLOB nvme/
)

add_kernel_module(
	NAME char
	NEEDS GENERIC_CHAR
	SOURCES char/generic.cpp
)

add_kernel_module(
	NAME e1000
	NEEDS E1000
	AUTOGLOB e1000/
)


add_kernel_module(
	NAME ps2_hid
	NEEDS PS2_HID
	AUTOGLOB ps2/
)


add_kernel_module(
	NAME vga
	NEEDS VGA
	AUTOGLOB vga/
)


# add_kernel_module(
# 	NAME RTL8139
# 	NEEDS RTL8139
# 	AUTOGLOB RTL8139/
# )


add_kernel_module(
	NAME sb16
	NEEDS SB16
	AUTOGLOB sb16/
)


# add_kernel_module(NAME sb16)
add_kernel_module(
	NAME virtio
	AUTOGLOB virtio
)

add_kernel_module(
	NAME vmware
	NEEDS VMWARE_VIDEO
	AUTOGLOB vmware/
)



add_kernel_module(
	NAME bus_pci
	NEEDS X86
	SOURCES bus/pci/pci.cpp
)
