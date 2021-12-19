add_kernel_module(
	NAME sifive_uart0
	NEEDS RISCV
	SOURCES sifive.cpp
)


add_kernel_module(
	NAME ns16550a_uart
	SOURCES ns16550a.cpp
)
