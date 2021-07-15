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

#include <lwip/netif.h>

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

    /* The LWIP interface that this binds to */
    struct netif netif;

    ip4_addr_t ip; /* Ipv4 Address */
    ip4_addr_t nm; /* net mask */
    ip4_addr_t gw; /* Gateway */

    interface(const char *name);

    struct wait_queue pending_arp_queue;
    ck::vec<net::arp::hdr *> pending_arps;


    virtual ~interface(void) = 0;
    /* Initialize the device (set mac address,  */
    virtual void init() = 0;
    /* Send a raw packet. Return 0 for success, -ERRNO for error */
    virtual int send_raw(struct pbuf *buf) = 0;

    /* Pass a packet up the network stack */
    void handle_packet(struct pbuf *buf);

    /* Set this interface as the default */
    inline void set_default(void) { netif_set_default(&netif); }

    inline void set_up(void) { netif_set_up(&netif); }
    inline void set_down(void) { netif_set_down(&netif); }

    inline void set_link_up(void) { netif_set_link_up(&netif); }
    inline void set_link_down(void) { netif_set_link_down(&netif); }
  };

  net::interface *find_interface(net::macaddr);
  void each_interface(func<bool(const string &, net::interface &)>);

  int register_interface(const char *name, net::interface *);
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
