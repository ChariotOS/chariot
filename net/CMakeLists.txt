
add_kernel_module(
	CORE NAME networkstack
	SOURCES
		interface.cpp
		ipcsock.cpp
		macaddr.cpp
		sock.cpp
		packet.cpp
		local.cpp
		ipv4/ipv4.cpp
		ipv4/udp.cpp
)

if (CONFIG_LWIP)
	add_subdirectory(lwip)
endif()
