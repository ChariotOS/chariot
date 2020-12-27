#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#ifdef __cplusplus
extern "C" {
#endif


#include "socket.h"


typedef unsigned short in_port_t;
typedef unsigned int in_addr_t;
struct in_addr {
  in_addr_t s_addr;
};


/* members are in network byte order */
struct sockaddr_in {
  sa_family_t sin_family;
  unsigned char sin_len;
  in_port_t sin_port;
  struct in_addr sin_addr;
#define SIN_ZERO_LEN 8
  char sin_zero[SIN_ZERO_LEN];
};


struct in6_addr {
  union {
    unsigned int u32_addr[4];
    unsigned char u8_addr[16];
  } un;
#define s6_addr un.u8_addr
};

struct sockaddr_in6 {
  sa_family_t sin6_family;
  in_port_t sin6_port;
  unsigned int sin6_flowinfo;
  struct in6_addr sin6_addr;
  unsigned int sin6_scope_id;
};

struct ipv6_mreq {
  struct in6_addr ipv6mr_multiaddr;
  unsigned ipv6mr_interface;
};

#ifdef __cplusplus
}
#endif

#endif
