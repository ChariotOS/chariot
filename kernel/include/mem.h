#ifndef __MEM__
#define __MEM__

#include <asm.h>


int init_mem(void);


// API for the low bitmap allocator. The region this allocates to
// is between the low kernel and the high kernel.
// The code for this lives in the low kernel
void *alloc_page(void);
void free_page(void *);

#endif
