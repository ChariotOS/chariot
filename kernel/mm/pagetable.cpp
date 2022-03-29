#include <mm.h>
#include <thread.h>

mm::PageTable::PageTable() {
  // TODO: add to some global list
}


mm::PageTable::~PageTable() {
  // TODO: remove from some global list
}


void *mm::PageTable::translate(off_t va) {
  struct pte pte;

  off_t off = va & 0xFFF;
  off_t vpage = va & ~0xFFF;

  int res = get_mapping(vpage, pte);

  // printf("translate(%p) -> %d\n", vpage, res);

  off_t ppage = pte.ppn;
  if (ppage == 0) return NULL;

  return (void *)((ppage << 12) + off);
}
