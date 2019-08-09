#include <mem.h>
#include <paging.h>
#include <phys.h>
#include <printk.h>
#include <types.h>
#include "../../include/mobo/multiboot.h"

#define MAX_MMAP_ENTRIES 128

/*
 * Round x up to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

/*
 * Round x down to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_down(x, y) ((x) & ~((y)-1))

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

void *memcpy(void *dest, const void *src, u64 n) {
  for (u32 i = 0; i < n; i++) {
    ((u8 *)dest)[i] = ((u8 *)src)[i];
  }
  return dest;
}

extern char low_kern_start;
extern char low_kern_end;
extern char high_kern_start;
extern char high_kern_end;

static char *kernel_heap_lo = NULL;
static char *kernel_heap_hi = NULL;

// the boot heap is a static heap, where you cannot free.
extern u8 bootheap_start;
u64 bootheap_alloc_count = 0;
u8 *bootheap_hi = &bootheap_start;
void *bootheap_malloc(u64 size) {
  u8 *addr = bootheap_hi;

  size = round_up(size, 8);
  bootheap_hi += size;
  bootheap_alloc_count += size;

  printk("BOOTHEAP_MALLOC\n");

  return addr;
}

void bootheap_free(void *p) { /* NOP! */
}

void *bootheap_realloc(void *p, u64 ns) {
  printk("BOOTHEAP_REALLOC USED, DO NOT DO THAT!\n");
  while (1) {
  }
}

static void *(*current_malloc)(u64) = &bootheap_malloc;
static void (*current_free)(void *) = &bootheap_free;
static void *(*current_realloc)(void *, u64) = &bootheap_realloc;

void *kmalloc(u64 size) { return current_malloc(size); }
void kfree(void *ptr) { current_free(ptr); }
void *krealloc(void *ptr, u64 newsize) { return current_realloc(ptr, newsize); }

// just 128 memory regions. Any more and idk what to do
static struct mem_map_entry memory_map[128];
static struct mmap_info mm_info;

void init_mmap(u64 mbd) {
  struct multiboot_tag *tag;
  uint32_t n = 0;

  if (mbd & 7) {
    printk("ERROR: Unaligned multiboot info struct\n");
  }

  tag = (struct multiboot_tag *)(mbd + 8);
  while (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
    tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag +
                                   ((tag->size + 7) & ~7));
  }

  if (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
    printk("ERROR: no mmap tag found\n");
    while (1)
      ;
  }

  multiboot_memory_map_t *mmap;

  for (mmap = ((struct multiboot_tag_mmap *)tag)->entries;
       (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
       mmap = (multiboot_memory_map_t *)((u64)mmap +
                                         ((struct multiboot_tag_mmap *)tag)
                                             ->entry_size)) {
    if (n > MAX_MMAP_ENTRIES) {
      printk("Reached memory region limit!\n");
      while (1)
        ;
    }

    u64 start, end;

    start = round_up(mmap->addr, 4096);
    end = round_down(mmap->addr + mmap->len, 4096);

    memory_map[n].addr = start;
    memory_map[n].len = end - start;
    memory_map[n].type = mmap->type;

    printk("mmap[%u] - [%p - %p] <%lu bytes>\n", n, start, end, end - start);

    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
      mm_info.usable_ram += mmap->len;
    }

    if (end > (mm_info.last_pfn << 12)) {
      mm_info.last_pfn = end >> 12;
    }

    mm_info.total_mem += end - start;
    ++mm_info.num_regions;
    ++n;
  }
}

void init_dyn_mm(void);

int init_mem(u64 mbd) {
  // go detect all the ram in the system
  init_mmap(mbd);

  printk("detected %lu bytes of usable memory (%lu pages)\n",
         mm_info.usable_ram, mm_info.usable_ram / 4096);

  auto *kend = (u8 *)&high_kern_end;

  // setup memory regions
  for (int i = 0; i < mm_info.num_regions; i++) {
    auto &region = memory_map[i];
    auto *start = (u8 *)region.addr;
    auto *end = start + region.len;

    if (end < kend) continue;
    if (start < kend) {
      start = kend + PGSIZE;
    }

    phys::free_range(start, end);
    /*
    for (u8 *p = start; p + PGSIZE <= end; p += PGSIZE) {
      map_page(p, p);
    }
    */
  }

  // initialize dynamic memory first, before we have smaller pages
  init_dyn_mm();

  return 0;
}

void *alloc_id_page() { return phys::alloc(); }


static u8 *kheap_start = NULL;
static u64 kheap_size = 0;

void *kheap_lo(void) { return kheap_start; }
void *kheap_hi(void) { return kheap_start + kheap_size; }

#define PGROUNDUP(x) round_up(x, 4096)

/**
 * ksbrk - shift the end of the heap further.
 */
void *ksbrk(i64 inc) {
  u64 oldsz = kheap_size;
  u64 newsz = oldsz + inc;
  if (inc == 0) return kheap_start + oldsz;

  i64 a = PGROUNDUP(oldsz);
  for (; a < newsz; a += 4096) {
    void *pa = phys::alloc();
    map_page(kheap_start + a, pa);
  }
  u8 *p = (u8 *)kheap_start + oldsz;
  memset(p, 0, newsz - oldsz);
  kheap_size = newsz;
  return kheap_start + oldsz;
}

extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
extern int mm_init(void);

void init_dyn_mm(void) {
  // the kheap starts, virtually, after the last physical page.
  kheap_start = (u8 *)((u64)mm_info.last_pfn << 12);
  kheap_size = 0;

  ksbrk(0);

  current_malloc = mm_malloc;
  current_free = mm_free;
  current_realloc = mm_realloc;
  /*


  mm_init();
  */
}
