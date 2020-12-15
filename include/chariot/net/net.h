#pragma once

#ifndef __CHARIOT_NET_H
#define __CHARIOT_NET_H

#include <asm.h>
#include <func.h>
#include <map.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <vec.h>
#include <wait.h>

namespace net {

namespace eth {
struct hdr;
}

namespace arp {
struct hdr;
}

struct [[gnu::packed]] macaddr {
  macaddr();
  macaddr(const uint8_t data[6]);
  macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);
  ~macaddr();

  inline uint8_t operator[](int i) const {
    assert(i >= 0 && i < 6);
    return raw[i];
  }

  bool operator==(const macaddr &other) const;
  bool is_zero(void) const;
  void clear(void);

  // the raw data
  uint8_t raw[6];
};

// fwd decl
struct interface;

// interface operations. Same usage and initialization as file_ops
struct ifops {
  // init - initialize the interface (fill in the hwaddr field)
  bool (*init)(struct net::interface &);

  struct net::eth::hdr *(*get_packet)(struct net::interface &);
  bool (*send_packet)(struct net::interface &, void *, size_t);
};

/* an interface is an abstraction around link-laayers. For example, ethernet or
 * Wi-Fi.
 */
struct interface {
  const char *name;

  // the mac address
  macaddr hwaddr;

  uint32_t address = 0;
  uint32_t gateway = 0;
  uint32_t netmask = 0;

  struct net::ifops &ops;

  interface(const char *name, struct net::ifops &o);

	struct wait_queue pending_arp_queue;
  vec<net::arp::hdr *> pending_arps;
  // network ordered ip to mac
  map<uint32_t, net::macaddr> arp_table;

  // send information to a mac address over ethernet
  int send(net::macaddr, uint16_t proto /* host ordered */, void *data,
	   size_t len);
};

net::interface *find_interface(net::macaddr);
void each_interface(func<bool(const string &, net::interface &)>);

int register_interface(const char *name, struct net::ifops &ops);
struct net::interface *get_interface(const char *name);

uint16_t htons(uint16_t n);
uint32_t htonl(uint32_t n);

uint16_t ntohs(uint16_t n);
uint32_t ntohl(uint32_t n);

static inline uint16_t net_ord(uint16_t n) { return htons(n); }
static inline uint32_t net_ord(uint32_t n) { return htonl(n); }
static inline uint16_t host_ord(uint16_t n) { return ntohs(n); }
static inline uint32_t host_ord(uint32_t n) { return ntohl(n); }

void start(void);
};  // namespace net

#endif
