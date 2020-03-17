#include <arch.h>
#include <cpu.h>
#include <lock.h>
#include <mem.h>
#include <multiboot.h>
#include <phys.h>
#include <printk.h>
#include <types.h>


#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

multiboot_info_t *multiboot_info_ptr;

u8 *kheap_start = NULL;
u64 kheap_size = 0;

void *kheap_lo(void) { return kheap_start; }
void *kheap_hi(void) { return kheap_start + kheap_size; }

extern "C" void *malloc(size_t size);
extern "C" void free(void *ptr);
extern "C" void *realloc(void *ptr, size_t size);

extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

static spinlock s_allocator_lock;

static void alloc_lock(void) { s_allocator_lock.lock(); }

static void alloc_unlock(void) { s_allocator_lock.unlock(); }

void *kmalloc(u64 size) {
  alloc_lock();
  auto ptr = mm_malloc(size);
  alloc_unlock();
  return ptr;
}
void kfree(void *ptr) {
  alloc_lock();
  auto p = (u64)ptr;
  if (ptr != NULL) {
    if (!(p >= (u64)kheap_lo() && p < (u64)kheap_hi())) {
      // printk("invalid address passed into free: %p\n", ptr);
    } else
      mm_free(ptr);
  }
  alloc_unlock();
}

void *krealloc(void *ptr, u64 newsize) {
  alloc_lock();
  auto p = mm_realloc(ptr, newsize);
  alloc_unlock();
  return p;
}

// stolen from musl :)
void *memcpy(void *dest, const void *src, size_t n) {

  auto *s = (uint8_t*)src;
  auto *d = (uint8_t*)dest;
  for (size_t i = 0; i < n; i++) d[i] = s[i];
  return dest;
}
