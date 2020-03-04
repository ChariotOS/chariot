#pragma once

#ifndef __CHARIOT_NET_H
#define __CHARIOT_NET_H

#include <types.h>

namespace net {

// fwd decl
namespace eth {
struct packet;
}

// fwd decl
struct interface;

// interface operations. Same usage and initialization as file_ops
struct ifops {
  // init - initialize the interface (fill in the hwaddr field)
  bool (*init)(struct net::interface &);

  struct net::eth::packet *(*get_packet)(struct net::interface &);
  bool (*send_packet)(struct net::interface &, void *, size_t);
};

/* an interface is an abstraction around link-laayers. For example, ethernet or
 * Wi-Fi.
 */
struct interface {
  const char *name;

  // the mac address
  uint8_t hwaddr[6];

  uint32_t source;
  uint32_t gateway;

  struct net::ifops &ops;

  interface(const char *name, struct net::ifops &o);
};

int register_interface(const char *name, struct net::ifops &ops);
struct net::interface *get_interface(const char *name);

uint16_t htons(uint16_t n);
uint32_t htonl(uint32_t n);

uint16_t ntohs(uint16_t n);
uint32_t ntohl(uint32_t n);

};  // namespace net

#endif
