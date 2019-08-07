#include <mem.h>
#include <paging.h>
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

// the boot heap is a static heap, where you cannot free.
extern u8 bootheap_start;
u64 bootheap_alloc_count = 0;
u8 *bootheap_hi = &bootheap_start;
void *bootheap_malloc(u64 size) {
  u8 *addr = bootheap_hi;

  size = round_up(size, 8);
  bootheap_hi += size;
  bootheap_alloc_count += size;

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

static void init_bitmap(void);

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
void id_map_kernel_code(void);

int init_mem(u64 mbd) {
  // go detect all the ram in the system
  init_mmap(mbd);

  printk("detected %lu bytes of usable memory (%lu pages)\n",
         mm_info.usable_ram, mm_info.usable_ram / 4096);

  init_bitmap();

  // initialize dynamic memory first, before we have smaller pages
  init_dyn_mm();

  // now that we have a basic bitmap allocator, we need to setup the
  // new paging system. To do this, we need to allocate a new p4_table,
  // and id map the entire kernel code/memory into it

  id_map_kernel_code();

  return 0;
}

typedef u8 page_t[4096];

/**
 * the physical bitmap allocator is a simple singly-linked list
 * of phys_bitmap structures. Unfortunately, these structures must live
 * at the start of each region they represent, so they absorb an entire page
 * in the region they control.
 */
struct phys_bitmap {
  // how many pages are availible. Purely an optimization to avoid searching in
  // a full region
  u32 nfree;

  // how many pages this bitmap controls
  u32 pagec;  // in pages
  // a pointer to the begining of the region. To access a pointer to the nth
  // page, use `region + n`
  page_t *pagev;
  struct phys_bitmap *next;

  // the size of this field varies and is based on pagec. It's length can be
  // solved with:
  //    length = pagec / sizeof(u32);
  u32 *bits;
};

// one pbitmap for every memory region. Ideally this would live somewhere else.
static struct phys_bitmap *pmaps = NULL;

// starting at bit 0, set the nth bit
static void write_pbitmap_bit(struct phys_bitmap *pbm, u32 n, bool state) {
  u32 *seg = pbm->bits + (n >> 5);
  *seg = *seg | ((state & 1) << (n & 0x11111));
}

static bool read_pbitmap_bit(struct phys_bitmap *pbm, u32 n) {
  return (*(pbm->bits + (n >> 5)) & (1 << (n & 0x11111))) != 0;
}

void *alloc_page(void) {
  for (struct phys_bitmap *map = pmaps; map != NULL; map = map->next) {
    if (map->nfree != 0) {
      for (int i = 0; i < map->pagec; i++) {
        if (read_pbitmap_bit(map, i) == false) {
          write_pbitmap_bit(map, i, true);
          return map->pagev + i;
        }
      }
    }
  }
  printk("not found\n");
  return NULL;
}

void free_page(void *p) { return; }

static void init_bitmap(void) {
  // we need to walk over every mem_map_entry we know, and add
  // a phys_bitmap where it would be best. The big difficulty of this operation
  // is to correctly surround the kernel code.

  // because we are building a linked list, we need a fake starter node
  struct phys_bitmap *prev = NULL;

  for (int i = 0; i < mm_info.num_regions; i++) {
    u8 *lo = (u8 *)memory_map[i].addr;
    u8 *hi = lo + memory_map[i].len;

    // if the region starts before the kernel code, shift the start to be after
    // the kernel code
    if (lo <= (u8 *)&low_kern_start)
      lo = (u8 *)round_up((u64)&high_kern_end, 4096);

    if (lo >= hi) continue;

    u64 size = hi - lo;
    u32 pagec = size / 4096;

    struct phys_bitmap *next = bootheap_malloc(sizeof(struct phys_bitmap));

    next->bits = bootheap_malloc(pagec / 8);

    lo = (u8 *)round_up((u64)lo, 4096);

    next->nfree = pagec;
    next->pagec = pagec;
    next->pagev = (page_t *)lo;
    next->next = NULL;

    if (pmaps == NULL) {
      pmaps = next;
    }

    if (prev != NULL) prev->next = next;
    prev = next;
  }
}

void id_map_kernel_code(void) {
  // allocate the p4_table
  u64 *p4_table = alloc_page();
  for (int i = 0; i < 512; i++) p4_table[i] = 0;
  // recursively map the page directory
  p4_table[511] = (u64)p4_table | 0x3;
  // printk("New p4: %p\n", p4_table);

  // id map the high kern
  for (char *p = &low_kern_start; p <= &high_kern_end; p += 4096) {
    map_page_into(p4_table, p, p);
  }
  write_cr3((u64)p4_table);
  tlb_flush();
}

u8 *kheap_lo_ptr = NULL;
u8 *kheap_hi_ptr = NULL;

/**
 * ksbrk - shift the end of the heap further.
 * NOTE: This is *very* inefficient
 */
void *ksbrk(u64 inc) {
  printk("sbrk: %ld\n", inc);
  // if inc is negative, NOP
  if (inc < 0) return kheap_hi_ptr;

  u32 new_pages = inc / 4096;
  if (ALIGN(inc, 4096) != inc) new_pages++;

  printk("%d new pages\n", new_pages);

  return kheap_hi_ptr;
}

uint64_t sbreakc = 0;

typedef struct free_header_s {
  struct free_header_s *prev;
  struct free_header_s *next;
  struct free_header_s *perf_ptr;
} free_header_t;

free_header_t *small_blocks = NULL;

typedef u64 blk_t;

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#ifdef ALIGN
#undef ALIGN
#endif

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define HEADER_SIZE (ALIGN(sizeof(blk_t)))

#define OVERHEAD (ALIGN(sizeof(free_header_t)))

// define some linked list operations
#define ll_fwd(name) ((name) = (name)->next)

#define GET_FREE_HEADER(blk) ((free_header_t *)((char *)blk + HEADER_SIZE))
#define ADJ_SIZE(given) ((given < OVERHEAD) ? OVERHEAD : ALIGN(given))
#define GET_BLK(header) ((blk_t *)((char *)(header)-HEADER_SIZE))
#define IS_FREE(blk) (*(blk)&1UL)
#define GET_SIZE(blk) (*(blk) & ~1UL)
#define SET_FREE(blk) (*(blk) |= 1UL)
#define SET_USED(blk) (*(blk) &= ~1UL)
#define SET_SIZE(blk, newsize) (*(blk) = ((newsize) & ~1UL) | IS_FREE(blk))

#define NEXT_BLK(blk) (blk_t *)((char *)(blk) + GET_SIZE(blk) + HEADER_SIZE)

void init_dyn_mm(void) {
  kheap_lo_ptr = (u8 *)round_up((u64)bootheap_hi + 4096, 4096);

  kheap_hi_ptr = kheap_lo_ptr;

  free_header_t *bp = ksbrk(OVERHEAD);
  bp->next = bp;
  bp->prev = bp;
}

void *kheap_lo(void) { return kheap_lo_ptr; }
void *kheap_hi(void) { return kheap_hi_ptr; }

/**
 * returns if two blocks are adjacent or not
 */
static bool adjacent(blk_t *a, blk_t *b) { return (NEXT_BLK(a) == b); }

free_header_t *find_fit(size_t size) {
  if (size <= 128 && small_blocks != NULL) {
    free_header_t *ptr = small_blocks;
    small_blocks = ptr->perf_ptr;
    return ptr;
  }
  free_header_t *bp = kheap_lo();
  bp = bp->next;
  while (bp != kheap_lo()) {
    blk_t *blk = GET_BLK(bp);
    if (GET_SIZE(blk) >= size) {
      return bp;
    }
    bp = bp->next;
  }
  // nothing was found, return NULL instead I guess.
  return NULL;
}

static blk_t *mm_split(blk_t *blk, size_t size) {
  size_t current_size = GET_SIZE(blk);
  int64_t size_after = current_size - size - HEADER_SIZE;
  if (size_after < (int64_t)OVERHEAD) return blk;

  blk_t *split_block = (blk_t *)((char *)blk + HEADER_SIZE + size_after);
  SET_SIZE(blk, size_after);
  SET_SIZE(split_block, size);
  // return blk;
  return split_block;
}

static void *mm_malloc(size_t size) {
  if (size == 112) size = 128;
  if (size == 448) size = 512;
  if (size == 2040) size = 2048;
  if (size == 4072) size = 4096;

  // the user lied to us, the real size they want is is aligned.
  size = ADJ_SIZE(size);

  free_header_t *fit;
  fit = find_fit(size);
  blk_t *blk;

  if (fit == NULL) {
    // there wasn't a valid spot for this allocation. Noone likes it.
    // it'll make a new spot, with blackjack
    blk = ksbrk(HEADER_SIZE + size);
    sbreakc++;
    SET_SIZE(blk, size);
  } else {
    // there was a spot in the heap for the node, but it might be too big,
    // so I'll have to split the block
    //
    blk = GET_BLK(fit);
    blk_t *split_block = mm_split(blk, size);

    // if the split block is the same block there wasn't a split
    // because I place the new block at the end of the old big block
    if (split_block == blk) {
      fit->prev->next = fit->next;
      fit->next->prev = fit->prev;
    }
    blk = split_block;
  }

  SET_USED(blk);
#ifdef DEBUG
  printf("MALLOC\n");
  print_heap();
#endif
  return (char *)blk + HEADER_SIZE;
}

// attempt_free_fusion attemps to join all the free blocks around a free block
// and returns the new "fused" block
static inline blk_t *attempt_free_fusion(blk_t *this_blk) {
  free_header_t *fh = GET_FREE_HEADER(this_blk);
  free_header_t *p = fh->prev;
  free_header_t *n = fh->next;
  blk_t *prev_blk = GET_BLK(p);
  blk_t *next_blk = GET_BLK(n);
  if (adjacent(prev_blk, this_blk) && adjacent(this_blk, next_blk)) {
    // printf("PREV->CURR->NEXT\n");
    p->next = n->next;
    n->next->prev = p;
    size_t newsize = GET_SIZE(prev_blk) + HEADER_SIZE + GET_SIZE(this_blk) +
                     HEADER_SIZE + GET_SIZE(next_blk);
    SET_SIZE(prev_blk, newsize);

    return prev_blk;
  }

  if (adjacent(prev_blk, this_blk)) {
    p->next = fh->next;
    fh->next->prev = p;
    size_t newsize = GET_SIZE(prev_blk) + HEADER_SIZE + GET_SIZE(this_blk);
    SET_SIZE(prev_blk, newsize);

    return prev_blk;
  }

  if (false && adjacent(this_blk, next_blk)) {
    fh->next = n->next;
    n->next->prev = fh;
    size_t newsize = GET_SIZE(this_blk) + HEADER_SIZE + GET_SIZE(next_blk);
    SET_SIZE(this_blk, newsize);
    return this_blk;
  }
  return this_blk;
}

// hopefully tail call optimized...
static blk_t *get_next_free(blk_t *curr) {
  curr = NEXT_BLK(curr);
  while (1) {
    if ((void *)curr > kheap_hi()) return NULL;
    if (IS_FREE(curr)) return curr;
    curr = NEXT_BLK(curr);
  }
}

static void mm_free(void *ptr) {
  blk_t *blk = GET_BLK(ptr);
  free_header_t *this = ptr;
  blk_t *next_free = get_next_free(blk);

#ifdef DEBUG
  printf("FREEING\n");
  print_heap();
#endif

  // if we were at the end of the heap...
  if (next_free == NULL) {
    this->next = kheap_lo();
    this->prev = this->next->prev;
    this->next->prev = this;
    this->prev->next = this;
    SET_FREE(blk);
  } else {
    free_header_t *succ = GET_FREE_HEADER(next_free);
    this->next = succ;
    this->prev = succ->prev;
    // printf("%p, %p\n", succ->prev, mem_heap_lo());
    this->prev->next = this;
    succ->prev = this;

    blk = attempt_free_fusion(blk);
    SET_FREE(blk);
  }
  return;
}

static void *mm_realloc(void *ptr, size_t size) {
  size = ALIGN(size);

  blk_t *blk = GET_BLK(ptr);
  size_t old_size = GET_SIZE(blk);

  // Expand into wilderness...
  if ((void *)NEXT_BLK(blk) > kheap_hi()) {
    size_t growth_size = size - old_size;
    ksbrk(growth_size);
    SET_SIZE(blk, size);
    return ptr;
  }

  if (IS_FREE(NEXT_BLK(blk))) {
    size_t next_size = GET_SIZE(NEXT_BLK(blk));
    if (size <= next_size + old_size + HEADER_SIZE) {
      free_header_t *n = GET_FREE_HEADER(NEXT_BLK(blk));
      n->prev->next = n->next;
      n->next->prev = n->prev;
      SET_SIZE(blk, next_size + old_size + HEADER_SIZE);
      return GET_FREE_HEADER(blk);
    }
  }
  void *new = mm_malloc(size);
  memcpy(new, ptr, GET_SIZE(GET_BLK(ptr)));
  mm_free(ptr);
  return new;
}

