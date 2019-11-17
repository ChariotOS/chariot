#pragma once

#include <ptr.h>
#include <string.h>
#include <types.h>
#include <vec.h>


#define VPROT_READ (1 << 0)
#define VPROT_WRITE (1 << 1)
#define VPROT_EXEC (1 << 2)
#define VPROT_SUPER (1 << 3)

#define VACC_WRITE 1
#define VACC_READ 2
#define VACC_EXEC 4

namespace vm {

class region;  // fwd decl
class addr_space;

// a crazy simple wrapper around the physical memory allocator, except it frees
// the page when no more references are held
struct phys_page : public refcounted<phys_page> {
  u64 pa;
  ~phys_page(void);

  static ref<phys_page> alloc(void);
};

/**
 * a memory backing is a structure that allows overloading of physical memory
 * access. By default it just owns physical pages
 */
class memory_backing : public refcounted<phys_page> {
 public:
  memory_backing(int npags);
  virtual ~memory_backing() = 0;  // pure virtual

  virtual int fault(addr_space &, region &, int page, int flags);

  // every memory region has an array of npages refcounted physical pages
  vec<ref<phys_page>> pages;
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

  // regions have a backing structure which handles faults
  ref<vm::memory_backing> backing;

 public:
  region(string name, off_t, size_t, int prot);
};

class addr_space final : public refcounted<addr_space> {
 public:



  off_t base, limit;

  // lookup a region by its page number, returning null when there is no region
  vm::region *lookup(off_t page_number);

  // delete a region from its page number
  int delete_region(off_t page_number);

  int handle_pagefault(off_t page_number, int flags);

  // the page table for this address space.
  void *page_table;

  int schedule_mapping(void *va, u64 pa);

  addr_space(void);
  ~addr_space(void);

  int set_range(off_t base, off_t limit);

 protected:

  vec<unique_ptr<vm::region>> regions;

  struct pending_mapping {
    void *va;
    u64 pa;
  };

  u64 revision = 0;
  u64 kmem_revision = 0;
  vec<pending_mapping> pending_mappings;
};

};  // namespace vm
