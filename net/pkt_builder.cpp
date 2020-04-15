#include <net/net.h>
#include <phys.h>
#include <printk.h>
#include <errno.h>

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

	memcpy((char*)buffer + length, data, len);
	length += len;

	return 0;
}

void *net::pkt_builder::alloc(long size) {
	void *buf = (void*)((char*)buffer + length);

	length += size;
	return buf;
}
