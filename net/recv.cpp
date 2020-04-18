#include <mem.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <net/pkt.h>
#include <util.h>
#include <sched.h>

static void print_packet(ref<net::pkt_buff> &pbuf) {
  auto eth = pbuf->eth();
  auto arp = pbuf->arph();
  auto ip = pbuf->iph();


	printk(KERN_INFO "Raw Packet:\n");
	hexdump(pbuf->get(), pbuf->size(), true);

  printk("[eth] src: %A, dst: %A, type: %04x\n", eth->source, eth->destination,
	 net::ntohs(eth->type));

  if (arp != NULL) {
    if (arp->ha_len != 6 || arp->pr_len != 4) {
      printk("[arp] ignoring due to address sizes being wrong\n");
      return;
    }

    auto op = net::ntohs(arp->op);
    auto spa = net::ntohl(arp->spa);
    auto tpa = net::ntohl(arp->tpa);

    if (op == ARPOP_REQUEST) printk("[arp] 'Who has %I? Tell %I'\n", spa, tpa);
    printk("\n");
  }

  if (ip != NULL) {
    printk("[ipv4]\n");
  }
}

// When packets are recv'd by adapters, they are sent here for internal parsing
// and routing
void net::packet_received(ref<net::pkt_buff> pbuf) {
  auto buf = pbuf->get();

  print_packet(pbuf);

  kfree(buf);
}
