#include <errno.h>
#include <mem.h>
#include <net/in.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <net/socket.h>
#include <printk.h>

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

uint16_t net::ipv4::checksum(const net::ipv4::packet &p) {
  uint32_t sum = 0;
  uint16_t *s = (uint16_t *)&p;

  /* TODO: Checksums for options? */
  for (int i = 0; i < 10; ++i) sum += net::ntohs(s[i]);

  if (sum > 0xFFFF) sum = (sum >> 16) + (sum & 0xFFFF);

  return ~(sum & 0xFFFF) & 0xFFFF;
}

int net::udp::send_data(net::interface &i, uint32_t ip, uint16_t from,
			uint16_t to, void *data, uint16_t length) {
  char *pkt;

  struct net::eth::packet epkt {
    .destination = BROADCAST_MAC, .source = {0}, .type = net::htons(0x0800),
  };

  memcpy(epkt.source, i.hwaddr, 6);

  uint16_t _length = net::htons(sizeof(struct net::ipv4::packet) +
				sizeof(struct net::udp::packet) + length);
  uint16_t _ident = net::htons(1);

  struct net::ipv4::packet ipv4_out = {
			.header_len = 5,
			.version = 4,
      .dscp_ecn = 0, /* not setting either of those */
      .length = _length,
      .ident = _ident,
      .flags_fragment = 0,
      .ttl = 0x40,
      .protocol = IPV4_PROT_UDP,
      .checksum = 0, /* fill this in later */
      .source = 0x00000000,
      .destination = net::htonl(ip),
  };

  uint16_t checksum = net::ipv4::checksum(ipv4_out);
  ipv4_out.checksum = net::htons(checksum);

  /* Now let's build a UDP packet */
  net::udp::packet udp_out = {
      .source_port = net::htons(from),
      .destination_port = net::htons(to),
      .length = net::htons(sizeof(net::udp::packet) + length),
      // TODO: checksum
      .checksum = 0,
  };

  auto pktlen = sizeof(epkt) + sizeof(ipv4_out) + sizeof(udp_out) + length;

  pkt = (char *)kmalloc(pktlen);

  off_t offset = 0;
  memcpy(pkt + offset, &epkt, sizeof(epkt));
  offset += sizeof(epkt);

  memcpy(pkt + offset, &ipv4_out, sizeof(ipv4_out));
  offset += sizeof(ipv4_out);

  memcpy(pkt + offset, &udp_out, sizeof(udp_out));
  offset += sizeof(udp_out);

  memcpy(pkt + offset, data, length);

  i.ops.send_packet(i, pkt, pktlen);

  kfree(pkt);
  return 0;
}

int net::ipv4::datagram_connect(net::sock &sk, struct sockaddr *uaddr,
				int addr_len) {
  struct sockaddr_in *usin = (struct sockaddr_in *)uaddr;

  if (addr_len != sizeof(*usin)) return -EINVAL;

  printk("ipv4::dg_conn\n");
  sk.connected = true;

  return 0;
}

int net::ipv4::datagram_disconnect(net::sock &sk, int flags) {
  printk("ipv4::dg_diconn\n");
  if (sk.connected) {
    sk.connected = false;
  }
  return 0;
}
