#include <lock.h>
#include <map.h>
#include <net/net.h>
#include <printk.h>
#include <string.h>

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

  interfaces[name] = i;
  printk("[net] registered new interface '%s': %02x:%02x:%02x:%02x:%02x:%02x\n",
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
