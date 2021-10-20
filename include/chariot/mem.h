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

#define v2p(addr) (void *)((off_t)(addr) & ~(off_t)CONFIG_KERNEL_VIRTUAL_BASE)
#define p2v(addr) (void *)((off_t)(addr) | (off_t)CONFIG_KERNEL_VIRTUAL_BASE)

#define USERSPACE_HIGH_ADDR ((off_t)CONFIG_KERNEL_VIRTUAL_BASE - 1)


#define Tp2v(addr) (decltype(addr))(p2v(addr))
#define Tv2p(addr) (decltype(addr))(v2p(addr))


// placement new
inline void *operator new(size_t, void *ptr) { return ptr; }


extern int kmem_revision;

int init_mem(u64 mbd);
size_t mem_size();


// void *memcpy(void *dest, const void *src, u64 n);

// kernel dynamic memory allocation functions
// NOTE: all of the internals of this API will change, but the API will not
void *malloc(unsigned long size);
void *zalloc(unsigned long size);
void free(void *ptr);
void *realloc(void *ptr, unsigned long newsize);




void malloc_dump();

void init_kernel_virtual_memory();

// template <typename T>
// inline T *malloc() {
//   return (T *)malloc(sizeof(T));
// }



template <typename T>
inline T *calloc(int count) {
  return (T *)malloc(sizeof(T) * count);
}

// template <typename T>
// inline T *malloc(int count) {
//   return (T *)malloc(sizeof(T) * count);
// }
#endif
