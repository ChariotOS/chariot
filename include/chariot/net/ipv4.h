#pragma once

#ifndef __CHARIOT_IPV4_H
#define __CHARIOT_IPV4_H

#include <net/net.h>
#include <types.h>

#define IPV4_PROT_UDP 17
#define IPV4_PROT_TCP 6
#define DHCP_MAGIC 0x63825363

namespace net {

  uint16_t checksum(void *, uint16_t size);

  template <typename T>
  inline uint16_t checksum(T &thing) {
    return checksum((void *)&thing, sizeof(thing));
  }


  namespace ipv4 {
    struct hdr {
      uint8_t header_len : 4;
      uint8_t version : 4;
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

    uint16_t checksum(const net::ipv4::hdr &p);


    struct route {
      bool valid = false;
      net::interface *in;
      net::macaddr next_hop;  // mac address
    };

    struct route route_to(uint32_t ip_target, uint32_t ip_source);

  }  // namespace ipv4

  namespace udp {
    struct hdr {
      uint16_t source_port;
      uint16_t destination_port;
      uint16_t length;
      uint16_t checksum;
      uint8_t payload[];
    } __attribute__((packed));
  };  // namespace udp

  namespace dhcp {
    struct hdr {
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
    struct hdr {
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

  namespace icmp {
    struct hdr {
      uint8_t type; /* message type */
      uint8_t code; /* type sub-code */
      uint16_t checksum;
      union {
        struct {
          uint16_t id;
          uint16_t sequence;
        } echo;           /* echo datagram */
        uint32_t gateway; /* gateway address */
        struct {
          uint16_t __unused;
          uint16_t mtu;
        } frag; /* path mtu discovery */
      };
    };

  };  // namespace icmp

};  // namespace net

#endif
