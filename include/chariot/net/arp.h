#pragma once

#ifndef __CHARIOT_ARP_H
#define __CHARIOT_ARP_H

#include <net/net.h>
#include <net/sock.h>
#include <types.h>

#define ARPHRD_ETHER 1     /* ethernet hardware format */
#define ARPHRD_IEEE802 6   /* IEEE 802 hardware format */
#define ARPHRD_FRELAY 15   /* frame relay hardware format */
#define ARPHRD_IEEE1394 24 /* IEEE 1394 (FireWire) hardware format */

#define ARPOP_REQUEST 1    /* request to resolve address */
#define ARPOP_REPLY 2      /* response to previous request */
#define ARPOP_REVREQUEST 3 /* request protocol address given hardware */
#define ARPOP_REVREPLY 4   /* response giving protocol address */
#define ARPOP_INVREQUEST 8 /* request to identify peer */
#define ARPOP_INVREPLY 9   /* response identifying peer */

namespace net {

  namespace arp {
    struct hdr {
      uint16_t ha_type;  /* format of hardware address */
      uint16_t protocol; /* format of protocol address */
      uint8_t ha_len;    /* length of hardware address */
      uint8_t pr_len;    /* length of protocol address */
      uint16_t op;
      /*
       * The remaining fields are variable in size,
       * according to the sizes above.
       */
      net::macaddr sender_hw; /* sender hardware address */
      uint32_t sender_ip;     /* sender protocol address */
      net::macaddr target_hw; /* target hardware address */
      uint32_t target_ip;     /* target protocol address */
    } __attribute__((packed));
  };  // namespace arp
};    // namespace net

#endif
