#ifndef __PHYS_H__
#define __PHYS_H__

#include <types.h>
#include <mem.h>

namespace phys {


  // allocate a physical page
  void *alloc(int npages = 1);


  // free one page of physical memory
  void free(void*, int len = 1);


  void free_range(void *, void*);

  u64 nfree(void);

  u64 bytes_free(void);


  // allocate a single page for kernel use
  inline void *kalloc(void) {
    return p2v(phys::alloc(1));
  }
  inline void kfree(void *p) {
    return phys::free(v2p(p), 1);
  }
};

#endif
