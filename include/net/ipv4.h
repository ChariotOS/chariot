#pragma once

#ifndef __CHARIOT_IPV4_H
#define __CHARIOT_IPV4_H

#include <net/net.h>
#include <net/sock.h>
#include <types.h>

#define BROADCAST_MAC \
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
#define IPV4_PROT_UDP 17
#define IPV4_PROT_TCP 6
#define DHCP_MAGIC 0x63825363

namespace net {

uint16_t checksum(void *, uint16_t size);

template <typename T>
inline uint16_t checksum(T &thing) {
  return checksum((void *)&thing, sizeof(thing));
}

namespace eth {
struct packet {
  uint8_t destination[6];
  uint8_t source[6];
  uint16_t type;
  uint8_t payload[];
} __attribute__((packed));
};  // namespace eth

namespace ipv4 {
struct packet {
  uint8_t version_ihl;
  uint8_t dscp_ecn;
  uint16_t length;
  uint16_t ident;
  uint16_t flags_fragment;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint32_t source;
  uint32_t destination;
  uint8_t payload[];
} __attribute__((packed));

#define IP_REPR_LEN 17
uint32_t parse_ip(const char *ip);
void format_ip(uint32_t ip, char *dst);


int datagram_connect(net::sock &sk, struct sockaddr*, int alen);
int datagram_disconnect(net::sock &sk, int flags);

uint16_t checksum(const net::ipv4::packet &p);


}  // namespace ipv4

namespace udp {
struct packet {
  uint16_t source_port;
  uint16_t destination_port;
  uint16_t length;
  uint16_t checksum;
  uint8_t payload[];
} __attribute__((packed));

// just send some arbitrary data over the interface
// all fields are host-ordered
int send_data(net::interface &, uint32_t ip, uint16_t from, uint16_t to,
              void *data, uint16_t length);

};  // namespace udp

namespace dhcp {
struct packet {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;

  uint32_t xid;

  uint16_t secs;
  uint16_t flags;

  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t siaddr;
  uint32_t giaddr;

  uint8_t chaddr[16];

  uint8_t sname[64];
  uint8_t file[128];

  uint32_t magic;

  uint8_t options[];
} __attribute__((packed));
};  // namespace dhcp

namespace dns {
struct packet {
  uint16_t qid;
  uint16_t flags;
  uint16_t questions;
  uint16_t answers;
  uint16_t authorities;
  uint16_t additional;
  uint8_t data[];
} __attribute__((packed));
};  // namespace dns

namespace tcp {

struct header {
  uint16_t source_port;
  uint16_t destination_port;

  uint32_t seq_number;
  uint32_t ack_number;

  uint16_t flags;
  uint16_t window_size;
  uint16_t checksum;
  uint16_t urgent;

  uint8_t payload[];
} __attribute__((packed));

struct check_header {
  uint32_t source;
  uint32_t destination;
  uint8_t zeros;
  uint8_t protocol;
  uint16_t tcp_len;
  uint8_t tcp_header[];
};
};  // namespace tcp

};  // namespace net

#endif
