#pragma once

#ifndef __CHARIOT_ETH_H
#define __CHARIOT_ETH_H

#define ETH_HDR_SIZE 14
#define ETH_TRL_SIZE 4
#define ETH_FRAME_SIZE_MIN 64
#define ETH_FRAME_SIZE_MAX 1500
#define ETH_PAYLOAD_SIZE_MIN \
  (ETH_FRAME_SIZE_MIN - (ETH_HDR_SIZE + ETH_TRL_SIZE))
#define ETH_PAYLOAD_SIZE_MAX \
  (ETH_FRAME_SIZE_MAX - (ETH_HDR_SIZE + ETH_TRL_SIZE))

#define ETH_TYPE_IP 0x0800
#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV6 0x86dd

#define ETH_ADDR_LEN 6
#define ETH_ADDR_STR_LEN 18 /* "xx:xx:xx:xx:xx:xx\0" */

#define BROADCAST_MAC \
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#include <types.h>
#include <net/net.h>

namespace net {
namespace eth {
struct hdr {
	net::macaddr dst;
	net::macaddr src;
  uint16_t type;
  uint8_t payload[];
} __attribute__((packed));
};  // namespace eth
}  // namespace net

#endif
