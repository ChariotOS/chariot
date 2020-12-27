#include <chan.h>
#include <errno.h>
#include <fifo_buf.h>
#include <lock.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <net/eth.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <net/pkt.h>
#include <printk.h>
#include <sched.h>
#include <string.h>
#include <util.h>

#include <map.h>


static spinlock interfaces_lock;
static map<string, struct net::interface *> interfaces;

void net::each_interface(func<bool(const string &, net::interface &)> fn) {
  interfaces_lock.lock();
  for (auto &a : interfaces) {
    if (!fn(a.key, *a.value)) break;
  }
  interfaces_lock.unlock();
}


net::interface *net::find_interface(net::macaddr m) {
  for (auto &a : interfaces) {
    if (a.value->hwaddr == m) {
      return a.value;
    }
  }
  return nullptr;
}

int net::register_interface(const char *name, net::interface *i) {
  scoped_lock l(interfaces_lock);

  if (interfaces.contains(name)) {
    panic("interface by name '%s' already exists\n", name);
    return -EEXIST;
  }

  interfaces[name] = i;
  printk(KERN_INFO "[net] registered new interface '%s': %02x:%02x:%02x:%02x:%02x:%02x\n", name, i->hwaddr[0],
         i->hwaddr[1], i->hwaddr[2], i->hwaddr[3], i->hwaddr[4], i->hwaddr[5]);

  return 0;
}

struct net::interface *net::get_interface(const char *name) {
  scoped_lock l(interfaces_lock);
  return interfaces.get(name);
}

static __inline uint16_t __bswap_16(uint16_t __x) { return __x << 8 | __x >> 8; }

static __inline uint32_t __bswap_32(uint32_t __x) {
  return __x >> 24 | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000) | __x << 24;
}

// static __inline uint64_t __bswap_64(uint64_t __x) { return (__bswap_32(__x) + 0ULL) << 32 | __bswap_32(__x >> 32); }

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

static long total_bytes_recv = 0;

static void print_packet(ref<net::pkt_buff> &pbuf) {
  auto eth = pbuf->eth();
  auto arp = pbuf->arph();
  auto ip = pbuf->iph();

  total_bytes_recv += pbuf->size();
  printk(KERN_INFO "Raw Packet (%d bytes, %d total):\n", pbuf->size(), total_bytes_recv);
  hexdump(pbuf->get(), pbuf->size(), true);

  printk("[eth] src: %A, dst: %A, type: %04x\n", eth->src.raw, eth->dst.raw, net::ntohs(eth->type));

  if (arp != NULL) {
    if (arp->ha_len != 6 || arp->pr_len != 4) {
      printk("[arp] ignoring due to address sizes being wrong\n");
      return;
    }

    auto op = net::ntohs(arp->op);
    auto spa = net::ntohl(arp->sender_ip);
    auto tpa = net::ntohl(arp->target_ip);

    if (op == ARPOP_REQUEST) printk("[arp] 'Who has %I? Tell %I'\n", spa, tpa);
    printk("\n");
  }

  if (ip != NULL) {
    printk("[ipv4]\n");
  }
}

static void handle_packet(ref<net::pkt_buff> &pbuf) {
  auto eth = pbuf->eth();
  auto in = net::find_interface(eth->dst);
  if (in == NULL) return;

  auto arp = pbuf->arph();
  if (arp != NULL) {
    // lookup by mac address
    auto p = new net::arp::hdr;
    memcpy(p, arp, sizeof(*p));
    in->pending_arps.push(p);
    in->pending_arp_queue.wake_up_all();
    return;
  }
  print_packet(pbuf);
}

struct pending_packet {
  ref<net::pkt_buff> pbuf;
};
static chan<ref<net::pkt_buff>> pending_packets;




int task(void *) {
  // initialize all the network interfaces
  //  TODO: this is probably a bad idea, since we are assuming that these addrs
  //  are safe without asking DHCP first.
  int octet = 15;
  net::each_interface([&](const string &name, net::interface &i) -> bool {
    i.address = net::net_ord(net::ipv4::parse_ip(string::format("10.0.0.%d", octet++).get()));
    i.netmask = net::net_ord(net::ipv4::parse_ip("255.255.255.0"));
    i.gateway = net::net_ord(net::ipv4::parse_ip("10.0.2.2"));

    /*
printk(KERN_INFO "[network task] Setup interface '%s'\n", name.get());
printk(KERN_INFO "               hardware: %A\n", i.hwaddr.raw);
printk(KERN_INFO "               address:  %I\n", net::host_ord(i.address));
printk(KERN_INFO "               netmask:  %I\n", net::host_ord(i.netmask));
printk(KERN_INFO "               gateway:  %I\n", net::host_ord(i.gateway));
    */

    return true;
  });
  while (1) {
    auto pbuf = pending_packets.recv();
    handle_packet(pbuf);
  }
  return 0;
}

volatile static int done = 0;
static semaphore tcpinit_sem(0);

static void donefunc(void *foo) {
  printk(KERN_DEBUG "Done!\n");
  done = 1;
  tcpinit_sem.post();
}


void net::start(void) {
  tcpip_init(donefunc, 0);
  if (tcpinit_sem.wait(false).interrupted()) {
    panic("unexpected interrupt");
  }

  // setup google dns for now, 8.8.8.8
  ip4_addr_t dns;
  dns.addr = net::net_ord(net::ipv4::parse_ip("8.8.8.8"));
  dns_setserver(0, &dns);
  // sched::proc::create_kthread("net task", task);
}

// When packets are recv'd by adapters, they are sent here for internal parsing
// and routing
void net::packet_received(ref<net::pkt_buff> pbuf) {
  printk("recv!\n");
  // skip non-ethernet packets
  auto eth = pbuf->eth();
  if (eth == NULL) return;

  pending_packets.send(move(pbuf));
}
