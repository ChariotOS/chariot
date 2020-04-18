#include <lock.h>
#include <map.h>
#include <net/eth.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <net/pkt.h>
#include <printk.h>
#include <sched.h>
#include <string.h>

#define OPT_PAD 0
#define OPT_SUBNET_MASK 1
#define OPT_ROUTER 3
#define OPT_DNS 6
#define OPT_REQUESTED_IP_ADDR 50
#define OPT_LEASE_TIME 51
#define OPT_DHCP_MESSAGE_TYPE 53
#define OPT_SERVER_ID 54
#define OPT_PARAMETER_REQUEST 55
#define OPT_END 255

// Message Types

#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK 5
#define DHCP_NAK 6
#define DHCP_RELEASE 7
#define DHCP_INFORM 8

static void test_interface(struct net::interface &i) {
	int sz = 5000;
  char *c = (char *)kmalloc(sz);
  net::ipv4::transmit(i, 0, 0, c, sz);
  kfree(c);


  net::pkt_builder b;

  // allocate all the headers
  auto &eth = b.alloc<net::eth::hdr>();
  auto &ip = b.alloc<net::ipv4::hdr>();
  auto &udp = b.alloc<net::udp::hdr>();
  auto &dhcp = b.alloc<net::dhcp::hdr>();

  // build the ethernet frame
  unsigned char bcast_mac[6] = BROADCAST_MAC;
  memcpy(eth.destination, bcast_mac, 6);
  memcpy(eth.source, i.hwaddr, 6);
  eth.type = net::htons(0x0800);

  ip.version = 4;
  ip.header_len = 5;  // 20 bytes
  ip.dscp_ecn = 0;
  ip.destination = net::htonl(net::ipv4::parse_ip("255.255.255.255"));
  ip.source = net::htonl(net::ipv4::parse_ip("10.0.2.15"));
  ip.protocol = 17;  // UDP
  ip.ttl = 255;

  udp.source_port = net::htons(68);
  udp.destination_port = net::htons(67);

  // const char *msg = "hello, world\n";
  // memcpy(b.alloc(strlen(msg) + 1), msg, strlen(msg) + 1);

  dhcp.op = 1;
  dhcp.hlen = 6;
  dhcp.hops = 0;
  dhcp.xid = 0;
  dhcp.magic = net::htonl(0x63825363);
  memcpy(dhcp.chaddr, i.hwaddr, 6);

  b.u8() = OPT_DHCP_MESSAGE_TYPE;
  b.u8() = 1;
  b.u8() = DHCP_DISCOVER;

  // Parameter Request list
  b.u8() = OPT_PARAMETER_REQUEST;
  b.u8() = 3;
  b.u8() = OPT_SUBNET_MASK;
  b.u8() = OPT_ROUTER;
  b.u8() = OPT_DNS;

  b.u8() = OPT_END;

  udp.length = net::htons(b.len_from(udp));
  udp.checksum = 0;

  ip.checksum = net::checksum(ip);
  ip.length = net::htons(b.len_from(ip));

  i.ops.send_packet(i, b.get(), b.size());
}

static spinlock interfaces_lock;
static map<string, struct net::interface *> interfaces;

net::interface::interface(const char *name, struct net::ifops &o)
    : name(name), ops(o) {
  assert(ops.init);
  assert(ops.get_packet);
  assert(ops.get_packet);

  ops.init(*this);
}

int net::register_interface(const char *name, struct net::ifops &ops) {
  scoped_lock l(interfaces_lock);

  if (interfaces.contains(name)) {
    panic("interface by name '%s' already exists\n", name);
    return -1;
  }
  auto i = new struct net::interface(name, ops);

  test_interface(*i);

  interfaces[name] = i;
  printk(KERN_INFO
	 "[net] registered new interface '%s': %02x:%02x:%02x:%02x:%02x:%02x\n",
	 name, i->hwaddr[0], i->hwaddr[1], i->hwaddr[2], i->hwaddr[3],
	 i->hwaddr[4], i->hwaddr[5]);

  return 0;
}

struct net::interface *net::get_interface(const char *name) {
  scoped_lock l(interfaces_lock);
  return interfaces.get(name);
}

static __inline uint16_t __bswap_16(uint16_t __x) {
  return __x << 8 | __x >> 8;
}

static __inline uint32_t __bswap_32(uint32_t __x) {
  return __x >> 24 | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000) | __x << 24;
}

static __inline uint64_t __bswap_64(uint64_t __x) {
  return (__bswap_32(__x) + 0ULL) << 32 | __bswap_32(__x >> 32);
}

#define bswap_16(x) __bswap_16(x)
#define bswap_32(x) __bswap_32(x)
#define bswap_64(x) __bswap_64(x)
uint16_t net::htons(uint16_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_16(n) : n;
}

uint32_t net::htonl(uint32_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_32(n) : n;
}

uint16_t net::ntohs(uint16_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_16(n) : n;
}

uint32_t net::ntohl(uint32_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_32(n) : n;
}
