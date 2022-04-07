
add_kernel_module(
	NAME sifive_spi
	# NEEDS NS16550_UART
	SOURCES sifive.cpp
)
