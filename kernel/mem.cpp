#include <arch.h>
#include <cpu.h>
#include <kshell.h>
#include <lock.h>
#include <mem.h>
#include <module.h>
#include <multiboot.h>
#include <phys.h>
#include <printk.h>
#include <types.h>
#include "lib/liballoc_1_1.h"

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))


multiboot_info_t *multiboot_info_ptr;

// bindings for liballoc
spinlock alloc_lock;

int liballoc_lock() {
  alloc_lock.lock();
  return 0;
}


int liballoc_unlock() {
  alloc_lock.unlock();
  return 0;
}


static unsigned long kmalloc_usage = 0;

void *liballoc_alloc(size_t s) {
  void *p = phys::kalloc(s);
  if (p == NULL) panic("Can't find region for malloc");
  kmalloc_usage += s * PGSIZE;

  // printk("liballoc_alloc(%zu) = %p  (total: %zu);\n", s, p, kmalloc_usage);
  return p;
}


int liballoc_free(void *buf, size_t sz) {
  phys::kfree(buf, sz);
  kmalloc_usage -= sz * PGSIZE;

  // printk("liballoc_free(%p, %zu)  (total: %zu);\n", buf, sz, kmalloc_usage);
  return 0;
}

extern "C" void *malloc(unsigned long s) { return kmalloc(s); }

extern "C" void free(void *p) { kfree(p); }


void *memcpy(void *dest, const void *src, size_t n) {
  auto *s = (uint8_t *)src;
  auto *d = (uint8_t *)dest;
  for (size_t i = 0; i < n; i++) d[i] = s[i];
  return dest;
}

int memcmp(const void *s1_, const void *s2_, size_t n) {
  const char *s1 = (const char *)s1_;
  const char *s2 = (const char *)s2_;

  while (n > 0) {
    int cmp = (*s1 - *s2);

    if (cmp != 0) {
      return cmp;
    }

    ++s1;
    ++s2;
    --n;
  }

  return 0;
}


int strncmp(const char *s1, const char *s2, size_t limit) {
  size_t i = 0;

  while (i < limit) {
    int cmp = (*s1 - *s2);

    if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
      return cmp;
    }

    ++s1;
    ++s2;
    ++i;
  }

  /* limit reached and equal */
  return 0;
}


char *strncpy(char *d, const char *s, size_t n) {
  for (; n && (*d = *s); n--, s++, d++) {
  }
  memset(d, 0, n);
  return d;
}


void *kzalloc(unsigned long sz) {
  auto p = kmalloc(sz);
  memset(p, 0, sz);
  return p;
}


static unsigned long mem_kshell(vec<string> &args, void *data, int dlen) {
  if (args.size() > 0) {
    if (args[0] == "dump") {
      printk("kmalloc usage: %zu bytes\n", kmalloc_usage);
      printk("physical free: %zu bytes\n", phys::bytes_free());
      return kmalloc_usage;
    }
  }
  return 0;
}
static void mem_init(void) { kshell::add("mem", mem_kshell); }



module_init("mem", mem_init);
