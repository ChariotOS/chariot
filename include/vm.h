#pragma once

#include <ptr.h>
#include <string.h>
#include <types.h>
#include <vec.h>

#define VPROT_READ (1 << 0)
#define VPROT_WRITE (1 << 1)
#define VPROT_EXEC (1 << 2)
#define VPROT_SUPER (1 << 3)

namespace vm {

class region;  // fwd decl
/**
 * a memory backing is a structure that allows overloading of physical memory
 * access.
 */
class memory_backing {
 public:
  virtual ~memory_backing() = 0;  // pure virtual

  /*
   * get a physical page address that can be mapped into the virtual address
   * space. This function is called once per
   */
  addr_t phys_page(region &, off_t offset);
};

/**
 * Virtual Regions define an extent of pages in a virtual address space
 */
class region {
 protected:
  friend class memory_backing;
  friend class addr_space;

  string name;

  // extent information, vpage is the virtual page number
  // len is how many pages the region extends over.
  off_t vpage;
  size_t len;

  // the protection of this region
  // memory protection is per-region and not per-page
  int prot = 0;

  // once the region has changed a page, the dirt flag is flipped to true. Which
  // causes the address space which owns this region to re-map the physical
  // addresses in the page table
  bool dirty = false;

  // regions have a backing structure which handles faults
  unique_ptr<vm::memory_backing> backing;

 public:
  region(string name, off_t, size_t, int prot);

  off_t fault(off_t);
};

class addr_space {
 public:
  // lookup a region by its page number, returning null when there is no region
  vm::region *lookup(off_t page_number);

  // delete a region from its page number
  int delete_region(off_t page_number);

  int handle_pagefault(off_t page_number, int flags);

 protected:
  vec<unique_ptr<vm::region>> regions;
};

};  // namespace vm
