#include <mem.h>
#include <printk.h>
#include <types.h>

#define N_BITS(n) ((1 << (n)) - 1)

/**
 * the physical memory allocator
 *
 * At a high level, the allocator works much like a page table, where
 * in order to find a physical address, you need to walk a series of tables
 * to eventually get a bitmap. Currently, you need to walk some tables of 512
 * pointers, and at the end is the bitmap of physical pages.
 *
 *
 * The "address" used to walk the tables is the physical page number, not
 * the address, which I will refer to as the "ppn"
 *
 *
 *
 * Each ppd has 512 addresses to other
 */

#define BITMAP_DIR_SHIFT 24
#define BITMAP_SHIFT 15

#define PAGES_PER_BITMAP (4096 * 8)

struct bitmap;

struct bitmap_dir {
  // 16,777,216 pages
  struct bitmap *maps[512];
};

struct bitmap {
  int nfree;

  // 4096 bytes, 32768 pages
  u8 map[4096];
};

// statically allocate the top level page dir
static struct bitmap_dir *page_dirs[512] = {
    [0 ... 511] = NULL,
};

u64 bm_alloc_size = 0;
static void *bmalloc(u64 size) {
  bm_alloc_size += size;
  return kmalloc(size);
}

static struct bitmap_dir *find_bitmap_dir(u64 ppn) {
  u64 ind = (ppn >> BITMAP_DIR_SHIFT) & N_BITS(9);

  struct bitmap_dir *dir = NULL;

  // check for the active bit
  if ((((u64)page_dirs[ind]) & 1) == 0) {
    printk("allocating a new ppage dir\n");
    // allocate the table
    dir = bmalloc(sizeof(struct bitmap_dir));
    // zero it out
    for (int i = 0; i < 512; i++) dir->maps[i] = 0;
    page_dirs[ind] = (struct bitmap_dir *)((u64)dir | 1);
  }

  return (struct bitmap_dir *)((u64)page_dirs[ind] & ~1);
}

struct bitmap *find_bitmap(u64 ppn) {
  u64 ind = (ppn >> BITMAP_SHIFT) & N_BITS(9);

  struct bitmap_dir *page_dir = find_bitmap_dir(ppn);

  struct bitmap *bmtable = NULL;

  // check for the active bit
  if ((((u64)page_dir->maps[ind]) & 1) == 0) {
    printk("allocating a new bitmap table\n");
    // allocate the table
    bmtable = bmalloc(sizeof(struct bitmap));
    for (int i = 0; i < 4096; i++) {
      bmtable->map[i] = 0;
    }
    bmtable->nfree = PAGES_PER_BITMAP;
    page_dir->maps[ind] = (struct bitmap *)((u64)bmtable | 1);
  }

  return (struct bitmap *)((u64)page_dir->maps[ind] & ~1);
}

bool ppn_check(u64 ppn) {
  // the lower 15 bits represent the bitmap offset
  u64 bit = ppn & N_BITS(BITMAP_SHIFT);
  u32 byte = bit / 8;
  u32 obit = bit % 8;

  struct bitmap *map = find_bitmap(ppn);

  return (map->map[byte] & (1 << obit)) != 0;
}

void ppn_reserve(u64 ppn) {
  // the lower 15 bits represent the bitmap offset
  u64 bit = ppn & N_BITS(BITMAP_SHIFT);
  u32 byte = bit / 8;
  u32 obit = bit % 8;

  struct bitmap *map = find_bitmap(ppn);

  // if the page was not used already, decrement the number of free pages
  if ((map->map[byte] & (1 << obit)) == 0) {
    map->nfree--;
  } else {
    // printk("[WARN] reserving already reserved physical page %ld\n", ppn);
  }

  map->map[byte] |= (1 << obit);
}

void ppn_release(u64 ppn) {
  // the lower 15 bits represent the bitmap offset
  u64 bit = ppn & N_BITS(BITMAP_SHIFT);
  u32 byte = bit / 8;
  u32 obit = bit % 8;

  struct bitmap *map = find_bitmap(ppn);

  // if the page was used already, increment the number of free pages
  if ((map->map[byte] & (1 << obit)) != 0) {
    map->nfree++;
  } else {
    // printk("[WARN] releasing already released physical page %ld\n", ppn);
  }

  map->map[byte] &= ~(1 << obit);
}

// allocate a physical page number that hasn't been used yet.
u64 ppn_alloc(void) {
  // TODO: take from the stack

  for (u64 ppn = 0; true; ppn += PAGES_PER_BITMAP) {
    struct bitmap *map = find_bitmap(ppn);
    // skip this map if it is all used up
    if (map->nfree == 0) {
      continue;
    }

    // find a bit in the massive bitmap
    for (int i = 0; i < 4096; i++) {
      for (int bit = 0; bit < 8; bit++) {
        u8 b = map->map[i];

        if ((b & (1 << bit)) == 0) {
          map->nfree--;
          map->map[i] |= (1 << bit);
          return ppn + (i * 8) + bit;
        }
      }
    }
  }
  return -1;
}

void ppn_free(u64 ppn) {
  // TODO: store the ppn in a stack of some sort
  ppn_release(ppn);
}

