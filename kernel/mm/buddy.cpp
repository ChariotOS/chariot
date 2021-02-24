#include <buddy.h>
#include <printk.h>


buddy_mempool::buddy_mempool(unsigned long base_addr, unsigned long pool_order, unsigned long min_order) {
	this->base_addr = base_addr;
	this->pool_order = pool_order;
	this->min_order = min_order;

	printk(KERN_INFO "Buddy mempool: %p, pool_order: %3d, min_order: %3d\n", base_addr, pool_order, min_order);
}


void buddy_mempool::free(void *addr, unsigned long order) {

}



void *buddy_mempool::malloc(unsigned long order) {
	return NULL;
}
