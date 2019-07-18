#ifndef __PAGING__
#define __PAGING__

#define PGERR ((void *)-1)
// Converts a virtual address into its physical address
//    This is some scary code lol
void *get_physaddr(void *va);

#endif
