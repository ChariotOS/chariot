#ifndef __MEM__
#define __MEM__

#include <asm.h>


struct mem_map_entry {
  uint64_t addr;
  uint64_t len;
  uint32_t type;
};

struct mmap_info {
  uint64_t usable_ram;
  uint64_t total_mem; /* includes I/O holes etc. */
  uint32_t last_pfn;
  uint32_t num_regions;
};

int init_mem(u64 mbd);

// API for the low bitmap allocator. The region this allocates to
// is between the low kernel and the high kernel.
// The code for this lives in the low kernel
void *alloc_page(void);
void free_page(void *);


void *memcpy(void *dest, const void *src, u64 n);



// kernel dynamic memory allocation functions
// NOTE: all of the internals of this API will change, but the API will not
void *kmalloc(u64 size);
void kfree(void *ptr);
void *krealloc(void *ptr, u64 newsize);

// increase the kernel heap
void *ksbrk(u64 inc);
void *kheap_lo();
void *kheap_hi();


// check if a physical page is used or not
bool ppn_check(u64 ppn);
// USE WITH CAUTION
void ppn_reserve(u64 ppn);
void ppn_release(u64 ppn);


// allocate and free ppns. This is the primary API
u64 ppn_alloc(void);
void ppn_free(u64);

#endif
