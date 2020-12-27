#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// We don't need v6
#define LWIP_IPV6 0

#define LWIP_IPV4 1
#define IP_FORWARD 1
#define IP_REASSEMBLY 1
#define IP_FRAG 1
#define IP_OPTIONS_ALLOWED 1
#define IP_REASS_MAXAGE 15
#define IP_REASS_MAX_PBUFS (10 << NUM_SHIFT)
#define IP_DEFAULT_TTL 255
#define IP_SOF_BROADCAST 0       // FIXME
#define IP_SOF_BROADCAST_RECV 0  // FIXME
#define IP_FORWARD_ALLOW_TX_ON_RX_NETIF 0

#define LWIP_DNS 1

#if 0
#define LWIP_DEBUG 1
// #define ETHARP_DEBUG LWIP_DBG_ON
// #define NETIF_DEBUG LWIP_DBG_ON
// #define PBUF_DEBUG LWIP_DBG_ON
// #define DHCP_DEBUG LWIP_DBG_ON
// #define DNS_DEBUG LWIP_DBG_ON
#define UDP_DEBUG LWIP_DBG_ON
// #define TCP_DEBUG LWIP_DBG_ON
#define SOCKETS_DEBUG LWIP_DBG_ON
// #define INET_DEBUG LWIP_DBG_ON
#endif


#define LWIP_RAW 1

#define NUM_SHIFT 4
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
// * DNS Options *
#define LWIP_DNS 1
#define DNS_TABLE_SIZE (4 << NUM_SHIFT)
#define DNX_MAX_NAME_LENGTH 256
#define DNS_MAX_SERVERS 2
#define DNS_MAX_RETRIES 4
#define DNS_DOES_NAME_CHECK 1
#define DNS_LOCAL_HOSTLIST 0             // FIXME
#define DNS_LOCAL_HOSTLIST_IS_DYNAMIC 0  // FIXME
#define LWIP_DNS_SUPPORT_MDNS_QUERIES 0

#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DHCP 1
#define DHCP_DOES_ARP_CHECK 1




// * Thread Options *
#define TCPIP_THREAD_NAME "[tcpip]"
#define DEFAULT_THREAD_NAME "[networkd]"




// #define DHCP_GLOBAL_XID                     1
// #define LWIP_DHCP_GET_NTP_SRV               1 // FIXME
// #define LWIP_DHCP_MAX_NTP_SERVERS           (1  << NUM_SHIFT)


// We use chariot system, so have threads to run in OS mode
#define NO_SYS 0

// We want the embedded loopback interface
#define LWIP_HAVE_LOOPIF 1

// We want it to use NK's malloc
// not that MEM_USE_POOLS/etc is off, so all allocation goes via NK
// we're not concerned about speed yet
#define MEM_LIBC_MALLOC 1

#define LWIP_COMPAT_SOCKETS 0
#undef LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS




// New after here

// new - do core locking
#define LWIP_TCPIP_CORE_LOCKING 1

// put the tcpip thread on a large stack regardless of
// compile-time config of NK
#define TCPIP_THREAD_STACKSIZE (2 * 1024 * 1024)
// nice big mbox - we have plenty of memory
#define TCPIP_MBOX_SIZE 128

// Make everything else nice and chunky
#define DEFAULT_THREAD_STACKSIZE (2 * 1024 * 1024)
#define DEFAULT_RAW_RECVMBOX_SIZE 128
#define DEFAULT_UDP_RECVMBOX_SIZE 128
#define DEFAULT_TCP_RECVMBOX_SIZE 128
#define DEFAULT_ACCEPTMBOX_SIZE 128
#endif
