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
  for (int i = 0; i < 10; ++i) sum += net::ntohs(s[i]);

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

net::ipv4sock::ipv4sock(int d, int t, int p) : net::sock(d, t, p) {
  // printk("ipv4sock\n");
}

net::ipv4sock::~ipv4sock(void) {}

struct net::ipv4::route net::ipv4::route_to(uint32_t target_addr, uint32_t source_addr) {
  /*
           printk("route %I -> %I\n", net::host_ord(target_addr),
           net::host_ord(source_addr));
           */

  struct net::ipv4::route r;
  r.in = NULL;

  net::interface *local_adapter = NULL;
  net::interface *gateway_adapter = NULL;

  net::each_interface([&](const string &name, net::interface &i) -> bool {
    auto adapter_addr = i.address;
    auto adapter_mask = i.netmask;

    if (source_addr != 0 && source_addr != adapter_addr) return true;

    if ((target_addr & adapter_mask) == (adapter_addr & adapter_mask)) local_adapter = &i;

    if (i.gateway != 0) gateway_adapter = &i;
    return true;
  });

  if (local_adapter && target_addr == local_adapter->address) {
    // printk(KERN_INFO "Routing to local_adapter\n");
    r.next_hop = local_adapter->hwaddr;
    r.in = local_adapter;
    return r;
  }

  if (!local_adapter && !gateway_adapter) {
    r.in = NULL;
    return r;
  }

  net::interface *adapter = NULL;
  uint32_t next_hop_ip = 0;
  if (local_adapter) {
    adapter = local_adapter;
    next_hop_ip = target_addr;
  } else if (gateway_adapter) {
    adapter = gateway_adapter;
    next_hop_ip = gateway_adapter->gateway;
  } else {
    // r is zero
    return r;
  }

  // if the adapter hasn't done DHCP yet, (address is still zero), we skip it
  // and the next hop is BROADCAST
  if (adapter->address == 0) {
    r.in = adapter;
    r.next_hop = BROADCAST_MAC;
    return r;
  }

  if (!arp_table.contains(target_addr)) {
    net::arp::hdr *arp_response = NULL;
    net::arp::hdr apkt;
    // ethernet
    apkt.ha_type = net::net_ord((uint16_t)1);
    apkt.protocol = net::net_ord((uint16_t)0x0800);
    apkt.ha_len = 6;
    apkt.pr_len = 4;
    apkt.op = net::net_ord((uint16_t)ARPOP_REQUEST);

    apkt.sender_hw = adapter->hwaddr;
    apkt.sender_ip = adapter->address;

    apkt.target_ip = next_hop_ip;

    adapter->send(BROADCAST_MAC, ETH_TYPE_ARP, &apkt, sizeof(apkt));

    while (1) {
      for (int i = 0; i < adapter->pending_arps.size(); i++) {
        auto *p = adapter->pending_arps[i];
        if (p->sender_ip == next_hop_ip) {
          adapter->pending_arps.remove(i);
          arp_table.set(target_addr, p->target_hw);
          arp_response = p;
          break;
        }
      }
      if (arp_response != NULL) {
        delete arp_response;
        break;
      }
      sched::yield();
      arch_relax();
    }
  }

  r.in = adapter;
  r.next_hop = adapter->arp_table.get(target_addr);
  return r;
}

ssize_t net::ipv4sock::send_packet(void *data, size_t len) {
  auto route = net::ipv4::route_to(peer_addr, local_addr);
  // printk("sending over %A\n", &route.next_hop.raw);
  if (route.in == NULL) {
    // not sure what to do here.
    printk("send_packet invalid route");
    return -EINVAL;
  }

  net::interface &i = *route.in;

  // printk("sending to %I over %A\n", peer_addr, route.next_hop.raw);
  // hexdump(data, len, true);

  pkt_builder b;
  // allocate all the headers
  auto &eth = b.alloc<net::eth::hdr>();
  auto &ip = b.alloc<net::ipv4::hdr>();

  eth.dst = route.next_hop;
  eth.src = route.in->hwaddr;
  eth.type = net::htons(0x0800);  // ipv4

  ip.version = 4;
  ip.header_len = 5;  // 20 bytes
  ip.dscp_ecn = 0;
  ip.destination = peer_addr;
  ip.source = route.in->address;
  ip.ident = net::net_ord(generate_ip_id());
  if (type == SOCK_DGRAM) {
    ip.protocol = 17;  // UDP
  } else {
    panic("uh");
  }
  ip.ttl = 64;  // dunno

  memcpy(b.alloc(len), data, len);

  ip.checksum = net::checksum(ip);
  ip.length = net::htons(b.len_from(ip));

  i.ops.send_packet(i, b.get(), b.size());

  return len;
}
