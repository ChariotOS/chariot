add_kernel_module(
	NAME ata
	NEEDS ATA X86
	SOURCES
		ata.cpp
)