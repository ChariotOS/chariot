#include <errno.h>
#include <net/net.h>
#include <net/pkt.h>
#include <phys.h>
#include <printk.h>

ref<net::pkt_buff> net::pkt_buff::create(void *data, size_t size) {
  return make_ref<net::pkt_buff>(data, size);
}

net::pkt_buff::pkt_buff(void *data, size_t size) {
  // 1 page (much less than 1500 MTU)
  buffer = phys::kalloc(1);
  length = size;
  memcpy(buffer, data, size);
}

net::eth::hdr *net::pkt_buff::eth(void) {
  return (net::eth::hdr *)buffer;
}

net::ipv4::hdr *net::pkt_buff::iph(void) {
  auto *eth = this->eth();
  if (eth->type == net::htons(0x0800)) return (net::ipv4::hdr *)(eth + 1);
  return NULL;
}

net::arp::hdr *net::pkt_buff::arph(void) {
  auto *eth = this->eth();
  if (eth->type == net::htons(0x0806)) return (net::arp::hdr *)(eth + 1);
  return NULL;
}

net::pkt_buff::~pkt_buff() {
  phys::kfree(buffer, 1);
}

net::pkt_builder::pkt_builder(void) {
  // 1 page (much less than 1500 MTU)
  buffer = phys::kalloc(1);
}

net::pkt_builder::~pkt_builder(void) {
  phys::kfree(buffer, 1);
}

int net::pkt_builder::append(void *data, int len) {
  if (length + len > 4096) {
    printk(KERN_WARN "pkt_builder::append() : E2BIG\n");
    return -E2BIG;
  }

  memcpy((char *)buffer + length, data, len);
  length += len;

  return 0;
}

void *net::pkt_builder::alloc(long size) {
  void *buf = (void *)((char *)buffer + length);

  length += size;
  return buf;
}
