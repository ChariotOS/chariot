#include <asm.h>
#include <paging.h>
#include <phys.h>
#include <printk.h>
#include <types.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define PGROUNDUP(x) round_up(x, 4096)

extern char high_kern_end[];
extern char phys_mem_scratch[];

struct frame {
  struct frame *next;
  u64 page_len;
};

static struct {
  int use_lock;
  frame *freelist = NULL;
  u64 nfree;
} kmem;


u64 phys::nfree(void) {
  return kmem.nfree;
}

static void print_freelist() {
  for (frame *f = kmem.freelist; f != NULL; f = f->next) {
  }
  printk("\n");
}

frame *tmp_map_frame(frame *fr) {
  map_page(phys_mem_scratch, fr);
  return (frame *)phys_mem_scratch;
}

// physical memory allocator implementation
void *phys::alloc(void) {
  frame *r = kmem.freelist;

  if (r == nullptr) panic("out of memory");

  frame *f = tmp_map_frame(r);

  if (f->page_len > 1) {
    auto n = (frame *)((char *)r + PGSIZE);

    auto *new_next = f->next;
    auto new_len = f->page_len - 1;

    kmem.freelist = n;

    n = tmp_map_frame(n);
    n->next = new_next;
    n->page_len = new_len;
  } else {
    kmem.freelist = f->next;
  }
  f = tmp_map_frame(r);
  memset(f, 0, PGSIZE);

  // decrement the number of freed pages
  kmem.nfree--;
  return r;
}

void phys::free(void *v) {
  frame *r;

  if ((u64)v % PGSIZE) panic("phys::free requires page aligned address");

  if (v < high_kern_end) panic("phys::free cannot free below the kernel's end");

  r = tmp_map_frame((frame*)v);
  r->next = kmem.freelist;
  r->page_len = 1;
  kmem.freelist = (frame *)v;

  // increment how many pages are freed
  kmem.nfree++;
}

// add page frames to the allocator
void phys::free_range(void *vstart, void *vend) {
  auto *fr = (frame *)PGROUNDUP((u64)vstart);

  u64 start_pn = PGROUNDUP((u64)vstart) >> 12;
  u64 end_pn = PGROUNDUP((u64)vend) >> 12;

  u64 pl = end_pn - start_pn;

  if (pl <= 0) {
    panic("zero free_range\n");
  }

  fr->page_len = pl;
  if (kmem.freelist == NULL) {
    fr->next = kmem.freelist;
    kmem.freelist = fr;
  } else {
    fr->next = kmem.freelist->next;
    kmem.freelist->next = fr;
  }
  kmem.nfree += fr->page_len;
}


