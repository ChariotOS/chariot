#ifndef __PAGING__
#define __PAGING__

#include <types.h>

#define PGERR ((void *)-1)


struct page_mapping {
  void *pa;
  bool valid;
};
// Converts a virtual address into its physical address
//    This is some scary code lol
struct page_mapping do_pagewalk(void *va);

void map_page_into(u64 *p4, void *va, void *pa);
void map_page(void *va, void *pa);

// given a 4th level page table, print it out :)
void print_page_dir(u64 *);

void *v2p(void* va);

#endif
