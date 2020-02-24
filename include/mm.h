#pragma once

#ifndef __MM_H_
#define __MM_H_

#include <ptr.h>
#include <vec.h>

#define VPROT_READ (1 << 0)
#define VPROT_WRITE (1 << 1)
#define VPROT_EXEC (1 << 2)
#define VPROT_SUPER (1 << 3)

#define FAULT_READ (1 << 0)
#define FAULT_WRITE (1 << 1)
#define FAULT_EXEC (1 << 2)
#define FAULT_PERM (1 << 3)
#define FAULT_NOENT (1 << 4)

namespace mm {

struct pte {
  off_t ppn;
  int prot;
};
/**
 * Page tables are created and implemented by the specific arch.
 * Implementations are found in arch/.../
 */
class pagetable : public refcounted<pagetable> {
 public:
  pagetable(void);
  virtual ~pagetable(void);
  virtual bool switch_to(void) = 0;

  virtual int add_mapping(off_t va, struct pte &) = 0;
  virtual int get_mapping(off_t va, struct pte &) = 0;
  virtual int del_mapping(off_t va) = 0;

  // implemented in arch, returns subclass
  static ref<pagetable> create();
};

struct vm_area {
  //
};

class vm_space {
  ref<mm::pagetable> pt;

  off_t lo, hi;

 public:
  vm_space(off_t lo, off_t hi);
  ~vm_space(void);


  mm::vm_area *lookup(off_t page_number);

  int delete_region(off_t page_number);
  int handle_pagefault(off_t va, int flags);
  int schedule_mapping(void *va, off_t pa, int prot);


 protected:
  vec<mm::vm_area *> regions;

  struct pending_mapping {
    off_t va;
    off_t pa;
    int prot;
  };

  unsigned long revision;
  unsigned long kmem_revision;
  vec<pending_mapping> pending_mappings;

};
};  // namespace mm

#endif
