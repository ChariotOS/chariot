#include <asm.h>
#include <lock.h>
#include <mem.h>
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

static mutex_lock phys_lck("physical allocator");


static void lock(void) {
  if (use_kernel_vm) phys_lck.lock();
}

static void unlock(void) {
  if (use_kernel_vm) phys_lck.unlock();
}

static struct {
  int use_lock;
  frame *freelist;
  u64 nfree;
} kmem;

u64 phys::nfree(void) { return kmem.nfree; }

u64 phys::bytes_free(void) { return nfree() << 12; }

/*
static void print_freelist() {
  for (frame *f = kmem.freelist; f != NULL; f = f->next) {
  }
  printk("\n");
}
*/

static frame *working_addr(frame *fr) {
  if (use_kernel_vm) {
    fr = (frame *)p2v(fr);
  } else {
    paging::map((u64)phys_mem_scratch, (u64)fr);
    fr = (frame *)phys_mem_scratch;
  }
  return fr;
}

static void *early_phys_alloc(void) {
  frame *r = kmem.freelist;
  if (r == nullptr) panic("out of memory");

  frame *f = working_addr(r);

  if (f->page_len > 1) {
    auto n = (frame *)((char *)r + PGSIZE);

    auto *new_next = f->next;
    auto new_len = f->page_len - 1;

    kmem.freelist = n;

    n = working_addr(n);
    n->next = new_next;
    n->page_len = new_len;
  } else {
    kmem.freelist = f->next;
  }
  f = working_addr(r);
  memset(f, 0, PGSIZE);
  // decrement the number of freed pages
  kmem.nfree--;
  return r;
}

// physical memory allocator implementation
void *phys::alloc(int npages) {

  lock();


  if (npages > 1 && !use_kernel_vm) {
    panic(
        "unable to allocate contiguious physical pages before kernel vm is "
        "availible\n");
  }

  if (use_kernel_vm) {
  }



  auto p = early_phys_alloc();
  unlock();
  return p;
}

void phys::free(void *v) {
  lock();

  if ((u64)v % PGSIZE) panic("phys::free requires page aligned address");
  if (v < high_kern_end) panic("phys::free cannot free below the kernel's end");

  frame *r = working_addr((frame *)v);
  r->next = kmem.freelist;
  r->page_len = 1;
  kmem.freelist = (frame *)v;

  // increment how many pages are freed
  kmem.nfree++;

  unlock();
}

// add page frames to the allocator
void phys::free_range(void *vstart, void *vend) {
  lock();

  auto *fr = (frame *)PGROUNDUP((u64)vstart);

  u64 start_pn = PGROUNDUP((u64)vstart) >> 12;
  u64 end_pn = PGROUNDUP((u64)vend) >> 12;

  u64 pl = end_pn - start_pn;

  if (pl <= 0) {
    panic("zero free_range\n");
  }

  frame *df = working_addr(fr);

  df->page_len = pl;
  if (kmem.freelist == NULL) {
    df->next = kmem.freelist;
    kmem.freelist = fr;
  } else {
    df->next = kmem.freelist->next;
    kmem.freelist->next = fr;
  }
  kmem.nfree += df->page_len;
  unlock();
}

