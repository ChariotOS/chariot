#include <asm.h>
#include <cpu.h>
#include <fs.h>
#include <lock.h>
#include <mem.h>
#include <paging.h>
#include <phys.h>
#include <printk.h>
#include <types.h>

// #define PHYS_DEBUG

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define PGROUNDUP(x) round_up(x, 4096)

extern char high_kern_end[];
extern char phys_mem_scratch[];

struct frame {
  struct frame *next;
  u64 page_len;

  inline struct frame *getnext(void) { return (frame *)p2v(next); }
  inline void setnext(frame *f) { next = (frame *)v2p(f); }
};

static spinlock phys_lck;

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

static frame *working_addr(frame *fr) {
  if (use_kernel_vm) {
    fr = (frame *)p2v(fr);
  } else {
    paging::map((u64)phys_mem_scratch, (u64)fr);
    fr = (frame *)phys_mem_scratch;
  }
  return fr;
}

static void *early_phys_alloc(int npages) {
  frame *r = kmem.freelist;
  if (r == nullptr) panic("out of memory");

  frame *f = working_addr(r);

  if (f->page_len > npages) {
    auto n = (frame *)((char *)r + PGSIZE * npages);

    auto *new_next = f->next;
    auto new_len = f->page_len - npages;

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
  kmem.nfree -= npages;
  return r;
}

#ifdef PHYS_DEBUG
static void print_free(void) {
  if (use_kernel_vm) {
    size_t nbytes = kmem.nfree * 4096;
    printk("phys: %zu kb\n", nbytes / 1024);

    auto f = working_addr(kmem.freelist);
    while (1) {
      printk("   %p ", v2p(f));

      if (f->getnext() <= f && f->next != NULL) {
        printk("!!");
      } else {
        printk("  ");
      }

      printk(" %p ", (char *)v2p(f) + f->page_len * PGSIZE);
      printk(" %zu pages\n", f->page_len);
      if (f->next == NULL) break;
      f = f->getnext();
    }
  }
}
#endif

static void *late_phys_alloc(size_t npages) {
  // if (npages > 1) print_free();
  frame *p = NULL;
  frame *c = (frame *)p2v(kmem.freelist);

  if (v2p(c) == NULL) panic("OOM!\n");

  while (v2p(c) != NULL) {
    if (c->page_len >= npages) break;
    p = c;
    c = c->getnext();
  }

  void *a = NULL;

  if (c->page_len == npages) {
    // remove from start
    if (v2p(p) == NULL) {
      kmem.freelist = c->next;
    } else {
      p->setnext(c->getnext());
    }
    a = (void *)c;
  } else {
    // take from end
    c->page_len -= npages;
    a = (void *)((char *)c + c->page_len * PGSIZE);
  }

  // zero out the page(s)
  memset(p2v(a), 0x00, npages * PGSIZE);
  kmem.nfree -= npages;


  // printk("free ram: %lu Kb\n", kmem.nfree * PGSIZE / 1024);

  return v2p(a);
}

// physical memory allocator implementation
void *phys::alloc(int npages) {

	lock();
	// reclaim block cache if the free pages drops below 32 pages
	if (kmem.nfree < 32) {
		unlock();
		printk("gotta reclaim!\n");
		block::reclaim_memory();
		lock();
	}

  void *p = use_kernel_vm ? late_phys_alloc(npages) : early_phys_alloc(npages);

  unlock();
  return p;
}

void phys::free(void *v, int len) {
  if (!use_kernel_vm) {
    panic("phys::free(%p) before kernel vm enabled\n", v);
  }

  if ((u64)v % PGSIZE) {
    panic("phys::free requires page aligned address. Given %p", v);
  }
  if (v <= high_kern_end)
    panic("phys::free cannot free below the kernel's end");

  lock();

  /* find a spot to place the freed page */

  frame *r = working_addr((frame *)v);

  r->next = NULL;
  r->page_len = len;

  frame *prev = working_addr(kmem.freelist);

  // insert at the start if we haven't found a slot
  if (v2p(prev) == NULL) {
    r->setnext(kmem.freelist);
    kmem.freelist = (frame *)v2p(r);
  } else {
    while (1) {
      if (prev->next == NULL) break;
      if (prev->getnext() > r) break;
      prev = prev->getnext();
    }
    // insert this frame after the prev
    r->setnext(prev->next);
    prev->setnext(r);
  }

  // patch up
  auto f = working_addr(kmem.freelist);
  while (1) {
    if (v2p(f) == NULL || f->next == NULL) break;

    auto succ = f->getnext();
    auto after = (frame *)v2p((frame *)((char *)f + f->page_len * PGSIZE));

    if (f->next == after) {
      f->page_len += succ->page_len;
      f->setnext(succ->next);
    }
    if (f->next == NULL) break;
    f = f->getnext();
  }
  // increment how many pages are freed
  kmem.nfree += len;


  // printk("free ram: %lu Kb\n", kmem.nfree * PGSIZE / 1024);

  unlock();
}

// add page frames to the allocator
void phys::free_range(void *vstart, void *vend) {
  lock();

	printk("0x%p 0x%p\n", vstart, vend);

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
    kmem.freelist = (frame *)v2p(fr);
  } else {
    df->next = kmem.freelist->next;
    kmem.freelist->next = fr;
  }
  kmem.nfree += df->page_len;
  unlock();
}

