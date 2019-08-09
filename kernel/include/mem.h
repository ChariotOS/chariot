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

void *memcpy(void *dest, const void *src, u64 n);



// kernel dynamic memory allocation functions
// NOTE: all of the internals of this API will change, but the API will not
void *kmalloc(u64 size);
void kfree(void *ptr);
void *krealloc(void *ptr, u64 newsize);

// increase the kernel heap
void *ksbrk(i64 inc);
void *kheap_lo();
void *kheap_hi();

void *alloc_id_page(void);

#endif
