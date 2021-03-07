#pragma once

#ifndef __CHARIOT_NET_PKT_H
#define __CHARIOT_NET_PKT_H

#include <net/arp.h>
#include <net/ipv4.h>
#include <net/eth.h>
#include <ptr.h>
#include <types.h>

namespace net {

  struct pkt_buff : public refcounted<pkt_buff> {
   private:
    long length = 0;
    void *buffer = nullptr;  // allocated with physical memory (1 page is enough
   public:
    static ref<pkt_buff> create(void *data, size_t size);

    inline void *get(void) {
      return buffer;
    }
    inline long size(void) {
      return length;
    }

    pkt_buff(void *data, size_t size);
    ~pkt_buff();

    // functions to read out headers
    net::eth::hdr *eth(void);
    net::ipv4::hdr *iph(void);
    net::arp::hdr *arph(void);
  };

  // takes an owned buffer (newly allocated, most likely)
  void packet_received(ref<pkt_buff>);

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
    inline long size(void) {
      return length;
    }

    inline auto &u8(void) {
      return alloc<uint8_t>();
    }
    inline auto &u16(void) {
      return alloc<uint16_t>();
    }
    inline auto &u32(void) {
      return alloc<uint32_t>();
    }

    template <typename T>
    inline T &alloc(void) {
      return *(T *)alloc(sizeof(T));
    }

    void *alloc(long size);

    template <typename T>
    inline unsigned long len_from(const T &h) {
      return length - ((char *)&h - (char *)buffer);
    }

    inline void clear(void) {
      memset(buffer, 0, length);
      length = 0;
      buffer = 0;
    }

    pkt_builder();
    ~pkt_builder();
  };
}  // namespace net

#endif
