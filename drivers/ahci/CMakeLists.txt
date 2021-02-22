add_kernel_module(
	NAME ahci
	NEEDS AHCI X86
	SOURCES
		ahci.cpp
		sata.cpp
)