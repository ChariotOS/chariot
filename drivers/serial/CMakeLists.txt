add_kernel_module(
	NAME sifive_uart0
	NEEDS SIFIVE_UART
	SOURCES sifive.cpp
)


add_kernel_module(
	NAME ns16550a_uart
	NEEDS NS16550_UART
	SOURCES ns16550a.cpp
)
