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



// takes an owned buffer (newly allocated, most likely)
void packet_received(void *buf, size_t len);


class pkt_builder {
  long length = 0;
  void *buffer = nullptr;  // allocated with physical memory (1 page is enough
			   // for most MTU things)

 public:
  inline void *get(void) {
		return buffer;
	}

  template <typename T>
  inline int append(T &hdr) {
    return append((void *)&hdr, sizeof(T));
  }
  int append(void *data, int len);
  inline long size(void) { return length; }



	inline auto &u8(void) {
		return alloc<uint8_t>();
	}
	inline auto &u16(void) {
		return alloc<uint16_t>();
	}
	inline auto &u32(void) {
		return alloc<uint32_t>();
	}



	template<typename T>
		inline T &alloc(void) {
			return *(T*)alloc(sizeof(T));
		}


	void *alloc(long size);


	template <typename T>
	inline unsigned long len_from(const T &h) {
		return length - ((char*)&h - (char *)buffer);
	}

  pkt_builder();
  ~pkt_builder();
};

};  // namespace net

#endif
