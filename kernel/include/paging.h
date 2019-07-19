#ifndef __PAGING__
#define __PAGING__

#include <types.h>

#define PGERR ((void *)-1)
// Converts a virtual address into its physical address
//    This is some scary code lol
void *get_physaddr(void *va);


void map_page_into(u64 *p4, void *va, void *pa);
void map_page(void *va, void *pa);



#endif
