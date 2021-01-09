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
  uint64_t nbytes;  // how many bytes

  off_t vmlo = 0;
  off_t vmhi = 0;
};

extern bool use_kernel_vm;

#define v2p(addr) (void *)((off_t)(addr) & ~(off_t)CONFIG_KERNEL_VIRTUAL_BASE)
#define p2v(addr) (void *)((off_t)(addr) | (off_t)CONFIG_KERNEL_VIRTUAL_BASE)

// placement new
inline void *operator new(size_t, void *ptr) { return ptr; }


extern int kmem_revision;

int init_mem(u64 mbd);
size_t mem_size();


// void *memcpy(void *dest, const void *src, u64 n);

// kernel dynamic memory allocation functions
// NOTE: all of the internals of this API will change, but the API will not
void *kmalloc(unsigned long size);
void *kzalloc(unsigned long size);
void kfree(void *ptr);
void *krealloc(void *ptr, unsigned long newsize);

void init_kernel_virtual_memory();

#endif
