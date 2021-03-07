#include <errno.h>
#include <lock.h>
#include <mem.h>
#include <net/eth.h>
#include <net/in.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <net/pkt.h>
#include <net/socket.h>
#include <printk.h>
#include <sched.h>
#include <util.h>

static map<uint32_t, net::macaddr> arp_table;

uint32_t net::ipv4::parse_ip(const char *name) {
  // TODO: not sure this is the smartest way to do this.
  uint32_t out[4];
  sscanf(name, "%d.%d.%d.%d", &out[0], &out[1], &out[2], &out[3]);
  return ((out[0] << 24) | (out[1] << 16) | (out[2] << 8) | (out[3]));
}

void net::ipv4::format_ip(uint32_t ip, char *dst) {
  // we expect dst to be at least 17 bytes
  uint8_t *v = (uint8_t *)&ip;
  sprintk(dst, "%d.%d.%d.%d", v[3], v[2], v[1], v[0]);
}

uint16_t net::checksum(void *p, uint16_t nbytes) {
  auto *ptr = (uint16_t *)p;

  long sum = 0;
  unsigned short oddbyte;

  while (nbytes > 1) {
    sum += *ptr++;
    nbytes -= 2;
  }
  if (nbytes == 1) {
    oddbyte = 0;
    *((unsigned char *)&oddbyte) = *(unsigned char *)ptr;
    sum += oddbyte;
  }

  sum = (sum >> 16) + (sum & 0xffff);
  sum = sum + (sum >> 16);
  return (uint16_t)~sum;
}

uint16_t net::ipv4::checksum(const net::ipv4::hdr &p) {
  uint32_t sum = 0;
  uint16_t *s = (uint16_t *)&p;

  /* TODO: Checksums for options? */
  for (int i = 0; i < 10; ++i)
    sum += net::ntohs(s[i]);

  if (sum > 0xFFFF) sum = (sum >> 16) + (sum & 0xFFFF);

  return ~(sum & 0xFFFF) & 0xFFFF;
}

static spinlock ip_id_lock;
static uint16_t generate_ip_id(void) {
  static uint16_t i = 128;
  ip_id_lock.lock();
  auto r = i++;
  ip_id_lock.unlock();
  return r;
}
