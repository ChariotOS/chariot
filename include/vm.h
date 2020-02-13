#pragma once

#include <lock.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <vec.h>
#include <fs.h>
#include <fs/vfs.h>

#define VPROT_READ (1 << 0)
#define VPROT_WRITE (1 << 1)
#define VPROT_EXEC (1 << 2)
#define VPROT_SUPER (1 << 3)


#define FAULT_READ (1 << 0)
#define FAULT_WRITE (1 << 1)
#define FAULT_EXEC (1 << 2)
#define FAULT_PERM (1 << 3)
#define FAULT_NOENT (1 << 4)


namespace vm {

class region;  // fwd decl
class addr_space;

// a crazy simple wrapper around the physical memory allocator, except it frees
// the page when no more references are held
struct phys_page : public refcounted<phys_page> {
  u64 pa = 0;
  ~phys_page(void);

  static ref<phys_page> alloc(void);
};

/**
 * a memory backing is a structure that allows overloading of physical memory
 * access. By default it just owns physical pages
 */
class memory_backing : public refcounted<phys_page> {
 public:
  memory_backing(int npages);
  virtual ~memory_backing();  // pure virtual

  virtual int fault(addr_space &, region &, int page, int flags);

  // every memory region has an array of npages refcounted physical pages
  vec<ref<phys_page>> pages;
};

class file_backing : public memory_backing {
  public:
  file_backing(int npages, size_t size, off_t, struct fs::inode *);
  virtual ~file_backing();
  virtual int fault(addr_space &, region &, int page, int flags);

  off_t off = 0;
  size_t size;
  fs::filedesc fd;
};

/**
 * Virtual Regions define an extent of pages in a virtual address space
 */
class region {
 public:
  string name;

  // extent information, vpage is the virtual page number
  // len is how many pages the region extends over.
  off_t va;
  size_t len;

  // the protection of this region
  // memory protection is per-region and not per-page
  int prot = 0;

  // regions have a backing structure which handles faults
  ref<vm::memory_backing> backing;
  region(string name, off_t, size_t, int prot);
};

class addr_space final : public refcounted<addr_space> {
 public:
  off_t base, limit;

  // lookup a region by its page number, returning null when there is no region
  vm::region *lookup(off_t page_number);

  // delete a region from its page number
  int delete_region(off_t page_number);

  int handle_pagefault(off_t va, int flags);

  // the page table for this address space.
  void *page_table;

  int schedule_mapping(void *va, u64 pa, int prot);

  addr_space(void);
  ~addr_space(void);

  addr_space(const addr_space&) = delete;

  int set_range(off_t base, off_t limit);

  off_t add_mapping(string name, off_t vaddr, size_t size, ref<vm::memory_backing>, int prot);
  off_t add_mapping(string name, ref<vm::memory_backing>, int prot);
  off_t add_mapping(string name, size_t sz, int prot);

  off_t map_file(string name, struct fs::inode *, off_t vaddr, off_t off, size_t size, int prot);

  off_t find_region_hole(size_t);

  void dump();



#define VALIDATE_READ 1
#define VALIDATE_WRITE 2
  bool validate_pointer(void *, size_t, int mode);
  bool validate_string(const char *);


  void *cr3;

  bool kernel_vm = false;

  // how much memory is used by the address space
  size_t resident = 0;

  inline const int region_count(void) const {
    return regions.size();
  }

  string format(void);

 protected:
  spinlock lck = spinlock("addr_space");

  vec<unique_ptr<vm::region>> regions;

  struct pending_mapping {
    void *va;
    u64 pa;
    int prot;
  };

  u64 revision = 0;
  u64 kmem_revision = 0;
  vec<pending_mapping> pending_mappings;

};




};  // namespace vm
