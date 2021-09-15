#include <mm.h>


mm::pagetable::pagetable() {
  // TODO: add to some global list
}


mm::pagetable::~pagetable() {
  // TODO: remove from some global list
}


void *mm::pagetable::translate(off_t va) {
	struct pte pte;

	off_t off = va & 0xFFF;
	off_t vpage = va & ~0xFFF;

	int res = get_mapping(vpage, pte);

	// printk("translate(%p) -> %d\n", vpage, res);

	off_t ppage = pte.ppn;
	if (ppage == 0) return NULL;

	return (void*)((ppage << 12) + off);
}

