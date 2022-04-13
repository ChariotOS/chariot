add_kernel_module(
	NAME pci-x86
	NEEDS X86
	SOURCES pci/pci-x86.cpp
)


add_kernel_module(
	NAME pci-fu740
	NEEDS RISCV
	SOURCES pci/pci-fu740.cpp
)


add_kernel_module(
	NAME pci-ecam
	SOURCES pci/pci-ecam-generic.cpp
)
