#include <asm.h>
#include <cpu.h>
#include <fs.h>
#include <lock.h>
#include <mem.h>
#include <phys.h>
#include <printf.h>
#include <time.h>
#include <types.h>
#include <syscall.h>
#include <thread.h>

#include <crypto.h>

// #define PHYS_DEBUG

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define PGROUNDUP(x) round_up(x, 4096)

extern char high_kern_end[];

struct frame {
  struct frame *next;
  u64 page_len;

  inline struct frame *getnext(void) { return (frame *)p2v(next); }
  inline void setnext(frame *f) { next = (frame *)v2p(f); }
};

static spinlock phys_lck;

static struct {
  int use_lock;
  frame *freelist;
  uint64_t nfree;    /* how many pages are currently free */
  uint64_t max_free; /* The maximum free memory we've seen */
} kmem;

u64 phys::nfree(void) { return kmem.nfree; }

u64 phys::bytes_free(void) { return nfree() << 12; }


int sys::getraminfo(unsigned long long *avail, unsigned long long *total) {
  if (!VALIDATE_WR(avail, sizeof(*avail))) return -EFAULT;
  if (!VALIDATE_WR(total, sizeof(*total))) return -EFAULT;
  // write to the location to cause a pagefault if it isnt mapped.
  (void)(*avail = 0);
  (void)(*total = 0);


  scoped_irqlock l(phys_lck);
  *avail = kmem.nfree << 12;
  *total = kmem.max_free << 12;
  return 0;
}


static frame *working_addr(frame *fr) { return (frame *)p2v(fr); }


static void *late_phys_alloc(size_t npages) {

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
  // zero out the page(s). This is relatively expensive
	uint64_t *buf = (uint64_t*)p2v(a);
	for (off_t i = 0; i < npages * PGSIZE / sizeof(uint64_t); i++) {
		buf[i] = 0;
	}
  kmem.nfree -= npages;

  return v2p(a);
}

// physical memory allocator implementation
void *phys::alloc(int npages) {
  // reclaim block cache if the free pages drops below 32 pages
  if (kmem.nfree < 32) {
    printf("gotta reclaim!\n");
    block::reclaim_memory();
  }

  scoped_irqlock l(phys_lck);
  void *p = late_phys_alloc(npages);
  return p;
}

void phys::free(void *v, int len) {
  if ((u64)v % PGSIZE) {
    panic("phys::free requires page aligned address. Given %p", v);
  }



  scoped_irqlock l(phys_lck);

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
  if (kmem.nfree > kmem.max_free) {
    kmem.max_free = kmem.nfree;
  }
	// printf_nolock("%llu/%lluMB\n", (kmem.nfree * 4096) / 1024 / 1024, kmem.max_free * 4096 / 1024 / 1024);
}

// add page frames to the allocator
void phys::free_range(void *vstart, void *vend) {
  scoped_irqlock l(phys_lck);

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
// #ifdef CONFIG_RISCV
//     auto fl = (frame *)p2v(kmem.freelist);
//     df->next = fl->next;
//     fl->next = fr;
// #else
    df->next = kmem.freelist->next;
    kmem.freelist->next = fr;
// #endif
  }
  kmem.nfree += df->page_len;
  if (kmem.nfree > kmem.max_free) {
    kmem.max_free = kmem.nfree;
  }
}
