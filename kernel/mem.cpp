#include <arch.h>
#include <cpu.h>
#include <kshell.h>
#include <lock.h>
#include <mem.h>
#include <module.h>
#include <util.h>
#include <phys.h>
#include <printk.h>
#include <types.h>
#include "lib/liballoc_1_1.h"

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))



extern "C" size_t strlen(const char *s) {
  if (s == NULL) return 0;
  const char *a = s;
  for (; *s; s++)
    ;
  return s - a;
}


// bindings for liballoc
spinlock alloc_lock;
static volatile long lock_flags = 0;
int liballoc_lock() {
  alloc_lock.lock();
  // lock_flags = alloc_lock.lock_irqsave();
  return 0;
}


int liballoc_unlock() {
  alloc_lock.unlock();
  // alloc_lock.unlock_irqrestore(lock_flags);
  return 0;
}


static unsigned long malloc_usage = 0;

void *liballoc_alloc(unsigned long s) {
  void *p = phys::kalloc(s);
  if (p == NULL) panic("Can't find region for malloc");
  malloc_usage += s * PGSIZE;

  // printk("alloc %4d  total: %12zu, free: %12zuB);\n", s, malloc_usage, phys::bytes_free());
  return p;
}


int liballoc_free(void *buf, unsigned long sz) {
  phys::kfree(buf, sz);
  malloc_usage -= sz * PGSIZE;

  // printk("free  %4d  (total: %zu, free: %zuB);\n", sz, malloc_usage, phys::bytes_free());
  return 0;
}




extern "C" void *memcpy(void *__restrict__ dest, const void *__restrict__ src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (unsigned char *)src;

#ifdef __GNUC__

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define LS >>
#define RS <<
#else
#define LS <<
#define RS >>
#endif

  typedef uint32_t __attribute__((__may_alias__)) u32;
  uint32_t w, x;

  for (; (uintptr_t)s % 4 && n; n--)
    *d++ = *s++;

  if ((uintptr_t)d % 4 == 0) {
    for (; n >= 16; s += 16, d += 16, n -= 16) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      *(u32 *)(d + 4) = *(u32 *)(s + 4);
      *(u32 *)(d + 8) = *(u32 *)(s + 8);
      *(u32 *)(d + 12) = *(u32 *)(s + 12);
    }
    if (n & 8) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      *(u32 *)(d + 4) = *(u32 *)(s + 4);
      d += 8;
      s += 8;
    }
    if (n & 4) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      d += 4;
      s += 4;
    }
    if (n & 2) {
      *d++ = *s++;
      *d++ = *s++;
    }
    if (n & 1) {
      *d = *s;
    }
    return dest;
  }

  if (n >= 32) switch ((uintptr_t)d % 4) {
      case 1:
        w = *(u32 *)s;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        n -= 3;
        for (; n >= 17; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 1);
          *(u32 *)(d + 0) = (w LS 24) | (x RS 8);
          w = *(u32 *)(s + 5);
          *(u32 *)(d + 4) = (x LS 24) | (w RS 8);
          x = *(u32 *)(s + 9);
          *(u32 *)(d + 8) = (w LS 24) | (x RS 8);
          w = *(u32 *)(s + 13);
          *(u32 *)(d + 12) = (x LS 24) | (w RS 8);
        }
        break;
      case 2:
        w = *(u32 *)s;
        *d++ = *s++;
        *d++ = *s++;
        n -= 2;
        for (; n >= 18; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 2);
          *(u32 *)(d + 0) = (w LS 16) | (x RS 16);
          w = *(u32 *)(s + 6);
          *(u32 *)(d + 4) = (x LS 16) | (w RS 16);
          x = *(u32 *)(s + 10);
          *(u32 *)(d + 8) = (w LS 16) | (x RS 16);
          w = *(u32 *)(s + 14);
          *(u32 *)(d + 12) = (x LS 16) | (w RS 16);
        }
        break;
      case 3:
        w = *(u32 *)s;
        *d++ = *s++;
        n -= 1;
        for (; n >= 19; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 3);
          *(u32 *)(d + 0) = (w LS 8) | (x RS 24);
          w = *(u32 *)(s + 7);
          *(u32 *)(d + 4) = (x LS 8) | (w RS 24);
          x = *(u32 *)(s + 11);
          *(u32 *)(d + 8) = (w LS 8) | (x RS 24);
          w = *(u32 *)(s + 15);
          *(u32 *)(d + 12) = (x LS 8) | (w RS 24);
        }
        break;
    }
  if (n & 16) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 8) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 4) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 2) {
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 1) {
    *d = *s;
  }
  return dest;
#endif

  for (; n; n--)
    *d++ = *s++;
  return dest;
}
extern "C" int memcmp(const void *__restrict__ s1_, const void *__restrict__ s2_, size_t n) {
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


extern "C" void memset(void *buf, char c, size_t len) {
  unsigned char *ptr = (unsigned char *)buf;
  for (size_t i = 0; i < len; i++) {
    ptr[i] = c;
  }
}


extern "C" void memset32(void *buf, uint32_t c, size_t len) {
  uint32_t *ptr = (uint32_t *)buf;
  for (size_t i = 0; i < len; i++) {
    ptr[i] = c;
  }
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

char *strcpy(char *d, const char *s) {
  for (; (*d = *s); s++, d++)
    ;

  return d;
}


void *zalloc(unsigned long sz) {
  auto p = malloc(sz);
  memset(p, 0, sz);
  return p;
}

static void insertion_sort(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
  int cnt;
  unsigned char ch;
  char *s1, *s2, *t1, *t2, *top;
  top = (char *)bot + nmemb * size;
  for (t1 = (char *)bot + size; t1 < top;) {
    for (t2 = t1; (t2 -= size) >= (char *)bot && compar(t1, t2) < 0;)
      ;
    if (t1 != (t2 += size)) {
      for (cnt = size; cnt--; ++t1) {
        ch = *t1;
        for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
          *s1 = *s2;
        *s1 = ch;
      }
    } else
      t1 += size;
  }
}

static void insertion_sort_r(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg) {
  int cnt;
  unsigned char ch;
  char *s1, *s2, *t1, *t2, *top;
  top = (char *)bot + nmemb * size;
  for (t1 = (char *)bot + size; t1 < top;) {
    for (t2 = t1; (t2 -= size) >= (char *)bot && compar(t1, t2, arg) < 0;)
      ;
    if (t1 != (t2 += size)) {
      for (cnt = size; cnt--; ++t1) {
        ch = *t1;
        for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
          *s1 = *s2;
        *s1 = ch;
      }
    } else
      t1 += size;
  }
}

extern "C" void qsort(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
  if (nmemb <= 1) return;

  insertion_sort(bot, nmemb, size, compar);
}


extern "C" void qsort_r(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg) {
  if (nmemb <= 1) return;

  insertion_sort_r(bot, nmemb, size, compar, arg);
}


static unsigned long mem_kshell(ck::vec<ck::string> &args, void *data, int dlen) {
  if (args.size() > 0) {
    if (args[0] == "dump") {
      printk("malloc usage: %zu bytes\n", malloc_usage);
      printk("physical free: %zu bytes\n", phys::bytes_free());
      return malloc_usage;
    }
  }
  return 0;
}
static void mem_init(void) { kshell::add("mem", "mem [dump]", mem_kshell); }



module_init("mem", mem_init);
