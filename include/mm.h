#pragma once

#ifndef __MM_H_
#define __MM_H_

#include <fs.h>
#include <mmap_flags.h>
#include <ptr.h>
#include <string.h>
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

// every physical page in mm circulation is kept track of via a heap-allocated
// `struct page`.
struct page : public refcounted<mm::page> {
  u64 pa = 0;
  /**
   * users is a representation of how many holders of this page there are.
   * This is useful for COW mappings, because you must copy the page on write if
   * and only if users > 1 (just you). If any other users exist, you copy it,
   * decrement the users count, and replace your reference with the new one
   */
  unsigned users = 0;
  int lock = 0;
  // some pages should not be phys::freed when this page is destructed.
  char owns_page = 0;

  ~page(void);

  static ref<page> alloc(void);
};

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

class space;

struct area {
  string name;

  off_t va;
  size_t len;  // in bytes
  off_t off;   // offset into the mapped file

  // flags from mmap (MAP_SHARED and co)
  int prot = 0;
  int flags = 0;

  spinlock lock;

  // TODO: unify shared mappings in the filedescriptor somehow
  ref<fs::filedesc> fd;
  vec<ref<mm::page>> pages;  // backing memory

  ~area(void);
};

class space {
  ref<mm::pagetable> pt;

  off_t lo, hi;

 public:
  space(off_t lo, off_t hi, ref<mm::pagetable>);
  ~space(void);

  void switch_to();
  mm::area *lookup(off_t va);

  int delete_region(off_t va);
  int pagefault(off_t va, int err);
  off_t mmap(off_t req, size_t size, int prot, int flags, ref<fs::filedesc>,
             off_t off);

  off_t mmap(string name, off_t req, size_t size, int prot, int flags,
             ref<fs::filedesc>, off_t off);
  int unmap(off_t addr, size_t sz);


  // returns the number of bytes resident
  size_t memory_usage(void);

  mm::space *fork(void);

#define VALIDATE_READ 1
#define VALIDATE_WRITE 2
  bool validate_pointer(void *, size_t, int mode);
  bool validate_string(const char *);

  // impl by arch::
  static mm::space &kernel_space(void);

  int is_kspace = 0;

 protected:
  int schedule_mapping(off_t va, off_t pa, int prot);
  int sort_regions(void);
  off_t find_hole(size_t size);

  spinlock lock;
  vec<mm::area *> regions;

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
