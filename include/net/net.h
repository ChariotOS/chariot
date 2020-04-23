#pragma once

#ifndef __CHARIOT_NET_H
#define __CHARIOT_NET_H

#include <asm.h>
#include <func.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <wait.h>
#include <map.h>
#include <vec.h>

namespace net {

namespace eth {
struct hdr;
}

namespace arp {
struct hdr;
}

struct [[gnu::packed]] macaddr {
  inline macaddr() {}
  inline macaddr(const u8 data[6]) { memcpy(raw, data, 6); }
  inline macaddr(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f) {
    raw[0] = a;
    raw[1] = b;
    raw[2] = c;
    raw[3] = d;
    raw[4] = e;
    raw[5] = f;
  }
  inline ~macaddr() {}

  inline u8 operator[](int i) const {
    ASSERT(i >= 0 && i < 6);
    return raw[i];
  }

  inline bool operator==(const macaddr &other) const {
    for (int i = 0; i < 6; i++) {
      if (raw[i] != other.raw[i]) return false;
    }
    return true;
  }

  inline bool is_zero() const {
    return raw[0] == 0 && raw[1] == 0 && raw[2] == 0 &&
	   raw[3] == 0 && raw[4] == 0 && raw[5] == 0;
  }

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


	waitqueue pending_arp_queue;
	vec<net::arp::hdr *> pending_arps;
	// network ordered ip to mac
	map<uint32_t, net::macaddr> arp_table;


	// send information to a mac address over ethernet
	int send(net::macaddr, uint16_t proto /* host ordered */, void *data, size_t len);
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

int task(void *);
};  // namespace net

#endif
