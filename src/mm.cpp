#include <mem.h>
#include <printk.h>
#include <types.h>
#include <vga.h>

uint64_t sbreakc = 0;

typedef struct free_header_s {
  struct free_header_s *prev;
  struct free_header_s *next;
  struct free_header_s *perf_ptr;
} free_header_t;

free_header_t *small_blocks = NULL;

typedef size_t blk_t;

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
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

// #define DEBUG
//
//

static inline void *heap_start(void) { return kheap_lo(); }

void print_heap(void) {
  uint64_t i = 0;
  auto *c = (blk_t *)((u8 *)kheap_lo() + OVERHEAD);
  for (; (void *)c < kheap_hi(); c = NEXT_BLK(c)) {
    bool free = IS_FREE(c);
    int blocks = GET_SIZE(c) / 8;
    for (int b = 0; b < blocks; b++) {
      vga::set_pixel(i + b, free ? 0x00FF00 : 0xFF0000);
    }
    i += blocks;
  }
}

// #define PRINT_DEBUG() print_heap()

#ifndef PRINT_DEBUG
#define PRINT_DEBUG()
#endif

static bool adjacent(blk_t *a, blk_t *b) { return (NEXT_BLK(a) == b); }

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  auto *bp = (free_header_t *)ksbrk(OVERHEAD);
  bp->next = bp;
  bp->prev = bp;
  return 0;
}

free_header_t *find_fit(size_t size) {
  if (size <= 128 && small_blocks != NULL) {
    free_header_t *ptr = small_blocks;
    small_blocks = ptr->perf_ptr;
    return ptr;
  }
  auto *bp = (free_header_t *)kheap_lo();
  bp = bp->next;
  while (bp != heap_start()) {
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
  i64 size_after = current_size - size - HEADER_SIZE;
  if (size_after < (i64)OVERHEAD) return blk;

  blk_t *split_block = (blk_t *)((char *)blk + HEADER_SIZE + size_after);
  SET_SIZE(blk, size_after);
  SET_SIZE(split_block, size);
  // return blk;
  return split_block;
}

void *mm_malloc(size_t size) {
  // the user lied to us, the real size they want is is aligned.
  size = ADJ_SIZE(size);

  free_header_t *fit;
  fit = find_fit(size);
  blk_t *blk;


  if (fit == NULL) {
    // there wasn't a valid spot for this allocation. Noone likes it.
    // it'll make a new spot, with blackjack
    blk = (blk_t *)ksbrk(HEADER_SIZE + size);
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
  void *ptr = (u8 *)blk + HEADER_SIZE;
  memset(ptr, 0, size);

  PRINT_DEBUG();
  return (char *)blk + HEADER_SIZE;
}

// attempt_free_fusion attemps to join all the free blocks around a free block
// and returns the new "fused" block
static inline blk_t *attempt_free_fusion(blk_t *this_blk) {
  free_header_t *fh = GET_FREE_HEADER(this_blk);
  free_header_t *p = fh->prev;
  free_header_t *n = fh->next;

  // avoid mucking up the kheap_lo and causing a PGFLT
  if (fh->prev == kheap_lo()) return this_blk;

  blk_t *prev_blk = GET_BLK(p);
  blk_t *next_blk = GET_BLK(n);
  if (adjacent(prev_blk, this_blk) && adjacent(this_blk, next_blk)) {
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

  if (adjacent(this_blk, next_blk)) {
    fh->next = n->next;
    n->next->prev = fh;
    size_t newsize = GET_SIZE(this_blk) + HEADER_SIZE + GET_SIZE(next_blk);
    SET_SIZE(this_blk, newsize);
    return this_blk;
  }
  return this_blk;
}

// hopefully tail call optimized...
blk_t *get_next_free(blk_t *curr) {
  curr = NEXT_BLK(curr);
  while (1) {
    if ((void *)curr >= kheap_hi()) return NULL;
    if (IS_FREE(curr)) return curr;
    curr = NEXT_BLK(curr);
  }
}

void mm_free(void *ptr) {
  if (ptr == nullptr) return;
  blk_t *blk = GET_BLK(ptr);
  auto *self = (free_header_t *)ptr;
  blk_t *next_free = get_next_free(blk);

  // if we were at the end of the heap...
  if (next_free == NULL) {
    self->next = (free_header_s *)heap_start();
    self->prev = self->next->prev;
    self->next->prev = self;
    self->prev->next = self;
    blk = attempt_free_fusion(blk);
    SET_FREE(blk);
  } else {
    free_header_t *succ = GET_FREE_HEADER(next_free);
    self->next = succ;
    self->prev = succ->prev;
    self->prev->next = self;
    succ->prev = self;

    blk = attempt_free_fusion(blk);
    SET_FREE(blk);
  }

  PRINT_DEBUG();
  return;
  size_t size = GET_SIZE(blk);
  free_header_t *freeheader = GET_FREE_HEADER(blk);
  if (size <= 128) {
    freeheader->perf_ptr = small_blocks;
    small_blocks = freeheader;
  }
}

void *mm_realloc(void *ptr, size_t size) {
  size = ALIGN(size);

  blk_t *blk = GET_BLK(ptr);
  size_t old_size = GET_SIZE(blk);

  // Expand into wilderness if the block is at the end of memory
  if ((void *)NEXT_BLK(blk) > kheap_hi()) {
    size_t growth_size = size - old_size;
    ksbrk(growth_size);
    SET_SIZE(blk, size);
    PRINT_DEBUG();
    return ptr;
  }

  /*
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
  */
  void *new_region = mm_malloc(size);
  memcpy(new_region, ptr, GET_SIZE(GET_BLK(ptr)));
  mm_free(ptr);
  PRINT_DEBUG();
  return new_region;
}
