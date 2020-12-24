
add_kernel_module(
	CORE NAME lwip
	NEEDS USE_LWIP
	GLOB **.cpp
)
