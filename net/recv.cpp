#include <mem.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <util.h>

static void print_mac(uint8_t *a) {
  printk("%02x:%02x:%02x:%02x:%02x:%02x", a[0], a[1], a[2], a[3], a[4], a[5]);
}

// When packets are recv'd by adapters, they are sent here for internal parsing
// and routing
void net::packet_received(void *buf, size_t len) {
  // assume it is ethernet...

  auto *eth = (eth::packet *)buf;

  printk("src: %A, dst: %A, type: %04x\n", eth->source, eth->destination, net::ntohs(eth->type));

  hexdump(buf, len, true);

  kfree(buf);
}
