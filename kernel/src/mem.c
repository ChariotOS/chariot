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

extern char low_kern_start;
extern char low_kern_end;
extern char high_kern_start;
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
  // simply clear out the bitmap
  for (int i = 0; i < 4096; i++) (&bitmap_start)[i] = 0xff;
}

int init_mem(void) {
  kernel_heap_lo = &high_kern_end;
  kernel_heap_hi = kernel_heap_lo;

  init_bitmap();
  // now that we have a basic bitmap allocator, we need to setup the
  // new paging system. To do this, we need to allocate a new p4_table,
  // and id map the entire kernel code/memory into it

  // allocate the p4_table
  u64 *p4_table = alloc_page();
  for (int i = 0; i < 512; i++) p4_table[i] = 0;
  // recursively map the page directory
  p4_table[511] = (u64)p4_table | 0x3;
  printk("New p4: %p\n", p4_table);

  // id map the lowkern
  for (char *p = &low_kern_start; p <= &low_kern_end; p += 4096) {
    map_page_into(p4_table, p, p);
  }
  // id map the high kern
  for (char *p = &high_kern_start; p <= &high_kern_end; p += 4096) {
    map_page_into(p4_table, p, p);
  }

  for (int i = 0; i < 512; i++) {
    if (p4_table[i] != 0) {
      printk("%4d %p\n", i, p4_table[i]);
    }
  }

  printk("\n\n");

  write_cr3((u64)p4_table);
  tlb_flush();

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
    unsigned char val = (&bitmap_start)[i];
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
