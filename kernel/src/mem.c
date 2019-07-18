#include <mem.h>
#include <paging.h>
#include <printk.h>
#include <types.h>

/**
 * The bitmap allocator is a low level, basic allocator that allows
 * allocation and freeing of whole pages within a somewhat small window
 * of memory
 *
 * It represents this memory by a metadata bitmap before the region, which
 * is a single page, where a 1 represents a free page, and a 0 indicates
 * a page is used somewhere
 */

// end of the kernel code
extern char high_kern_end;

static char *kernel_heap_lo = NULL;
static char *kernel_heap_hi = NULL;

extern char bitmap_metadata;
extern char bitmap_start;
extern char bitmap_end;

// Use sparingly
static void *next_page(void) {
  void *p = kernel_heap_hi;
  kernel_heap_hi += 4096;
  return p;
}

static void init_bitmap(void) {
  u64 entries = 4096 * 8;  // 1 bit for every page
  printk("%lu entries\n", entries);
  printk("%lu bytes\n", entries * 4096);

  // bitmap_start = kernel_heap_hi;
  // kernel_heap_hi += entries * 4096;
  // bitmap_end = kernel_heap_hi;

  // we need to clear the bitmap, as no pages are allocated
  for (int i = 0; i < 4096; i++) (&bitmap_start)[i] = 0xff;
}

int init_mem(void) {
  kernel_heap_lo = &high_kern_end;
  kernel_heap_hi = kernel_heap_lo;

  // bitmap_metadata = next_page();

  init_bitmap();

  return 0;
}





// #define DEBUG_BITMAP_ALLOCATOR
#ifndef DEBUG_BITMAP_ALLOCATOR
#define BIT_DEBUG(fmt, ...)
#else
#define BIT_DEBUG(fmt, ...) printk("[BITMAP] " fmt, ##__VA_ARGS__)
#endif

void print_bitmap_md(void) {
#ifdef DEBUG_BITMAP_ALLOCATOR
  printk("  ");
  for (int i = 0; i < 8; i++) {
    unsigned char val = bitmap_start[i];
    for (int o = 7; o >= 0; o--) {
      bool set = (val >> o) & 1;
      printk("%d", set);
    }
    printk(" ");
  }
  printk("\n");
#endif
}

void *alloc_page(void) {
  BIT_DEBUG("ALLOC:\n");
  print_bitmap_md();
  void *addr = NULL;
  for (int i = 0; i < 4096; i++) {
    unsigned char val = (&bitmap_start)[i];

    for (int o = 7; o >= 0; o--) {
      bool set = (val >> o) & 1;
      // the page we are looking at is free, so use it
      if (set) {
        // set the bit to set
        (&bitmap_start)[i] = val & ~(1 << o);
        addr = &bitmap_start + (i * 8 * 4096) + (4096 * (7 - o));
        goto FOUND;
      }
    }
  }

  // we never found one, oof
  return NULL;
FOUND:
  print_bitmap_md();
  BIT_DEBUG("  allocated %p\n\n", addr);
  return addr;
}

void free_page(void *p) {
  if (p < (void *)&bitmap_start || p > (void *)&bitmap_end) {
    printk("PAGE NOT OWNED BY BITMAP\n");
    return;
  };

  BIT_DEBUG("FREEING:\n");
  print_bitmap_md();

  u64 ind = ((char *)p - &bitmap_start) / 4096;

  int bit = 7 - (ind & 0xf);
  int byte = ind & ~0xf;

  char old = (&bitmap_start)[byte];
  (&bitmap_start)[byte] = old | (1 << bit);

  print_bitmap_md();
  BIT_DEBUG("\n\n");
}
