add_kernel_module(
	NAME bus_pci
	NEEDS X86
	SOURCES pci/pci.cpp
)