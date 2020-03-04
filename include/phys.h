#ifndef __PHYS_H__
#define __PHYS_H__

#include <types.h>
#include <mem.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define NPAGES(sz) (round_up((sz), 4096) / 4096)

namespace phys {


  // allocate a physical page
  void *alloc(int npages = 1);


  // free one page of physical memory
  void free(void*, int len = 1);


  void free_range(void *, void*);

  u64 nfree(void);

  u64 bytes_free(void);


  inline void *kalloc(int npages) {
    return p2v(phys::alloc(npages));
  }
  inline void kfree(void *p, int npages) {
    return phys::free(v2p(p), npages);
  }
};

#endif
