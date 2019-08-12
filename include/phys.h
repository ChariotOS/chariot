#ifndef __PHYS_H__
#define __PHYS_H__

#include <types.h>

namespace phys {


  // allocate a physical page
  void *alloc(void);


  // free one page of physical memory
  void free(void*);


  void free_range(void *, void*);

  u64 nfree(void);
};

#endif
