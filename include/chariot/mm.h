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

#define VALIDATE_RD(ptr, size) curproc->mm->validate_pointer((void *)ptr, size, PROT_READ)

#define VALIDATE_WR(ptr, size) curproc->mm->validate_pointer((void *)ptr, size, PROT_WRITE)

#define VALIDATE_RDWR(ptr, size) curproc->mm->validate_pointer((void *)ptr, size, PROT_WRITE | PROT_READ)

#define VALIDATE_EXEC(ptr, size) curproc->mm->validate_pointer((void *)ptr, size, PROT_EXEC)


namespace mm {


  // #define PAGE_DEBUG_LIST

  // every physical page in mm circulation is kept track of via a heap-allocated
  // `struct page`.
  struct page : public refcounted<mm::page> {
    bool dirty = false;
    unsigned long lru = 0;
    unsigned long pa = 0;
    /**
     * users is a representation of how many holders of this page there are.
     * This is useful for COW mappings, because you must copy the page on write
     * if and only if users > 1 (just you). If any other users exist, you copy
     * it, decrement the users count, and replace your reference with the new
     * one
     */
    unsigned users = 0;
    int lock = 0;

    // non-freeable pages are needed (for example when mmapping mmio regions)
    bool freeable : 1;
    bool writethrough : 1;
    bool nocache : 1;

    page(void);
    ~page(void);

    static ref<page> alloc(void);
    // create a page mapping for some physical memory
    // note: this page isn't owned.
    static ref<page> create(unsigned long pa);

#ifdef PAGE_DEBUG_LIST
    struct page *dbg_prev;
    struct page *dbg_next;
#endif
  };

  struct pte {
    off_t ppn;
    int prot;

    bool writethrough = false;
    bool nocache = false;
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

  // vm areas can optionally have a vmobject that represents them.
  struct vmobject : public refcounted<vmobject> {
    inline vmobject(size_t npages) : n_pages(npages) {}

    virtual ~vmobject(void){};

    // added and removed from spaces.
    inline void acquire(void) {
      scoped_lock l(m_lock);
      owners++;
    }
    inline void release(void) {
      scoped_lock l(m_lock);
      owners--;
      if (owners == 0) drop();
    }

    // all owners have dropped you!
    virtual void drop(void) {}

    // get a shared page (page #n in the mapping)
    virtual ref<mm::page> get_shared(off_t n) = 0;
    // tell the region to flush a page
    virtual void flush(off_t n) {}

    inline size_t size(void) { return n_pages * PGSIZE; }

   private:
    spinlock m_lock;
    int owners = 0;
    size_t n_pages;
  };


  struct area : public refcounted<area> {
    string name;

    off_t va;
    size_t len;  // in bytes
    off_t off;   // offset into the mapped file

    // flags from mmap (MAP_SHARED and co)
    int prot = 0;
    int flags = 0;

    spinlock lock;

    // TODO: unify shared mappings in the fileriptor somehow
    ref<fs::file> fd;
    vec<ref<mm::page>> pages;  // backing memory

    // optional. If it exists, it is queried for each page
    // This is required if the region is not anonymous. If a region is mapped
    // and it isn't anon but has no obj field, it acts like an anon mapping
    ref<vmobject> obj = nullptr;

    area(void);
    ~area(void);


    static inline int compare(area &a, area &b) { return a.va - a.va; }
  };

  class space {
   public:
    ref<mm::pagetable> pt;

    off_t lo, hi;

    space(off_t lo, off_t hi, ref<mm::pagetable>);
    ~space(void);


    size_t copy_out(off_t addr, void *into, size_t len);

    inline auto get_pt(void) { return pt; }
    void switch_to();
    mm::area *lookup(off_t va);

    int delete_region(off_t va);
    int pagefault(off_t va, int err);
    off_t mmap(off_t req, size_t size, int prot, int flags, ref<fs::file>, off_t off);

    off_t mmap(string name, off_t req, size_t size, int prot, int flags, ref<fs::file>, off_t off);
    int unmap(off_t addr, size_t sz);


    // returns the number of bytes resident
    size_t memory_usage(void);

    mm::space *fork(void);

#define VALIDATE_READ 1
#define VALIDATE_WRITE 2
    bool validate_pointer(void *, size_t, int mode);
    bool validate_string(const char *);


    template <typename T, typename V>
    bool validate_struct(V val, int mode) {
      return validate_pointer((void *)(val), sizeof(T), mode);
    }

    // impl by arch::
    static mm::space &kernel_space(void);

    void dump();

    int is_kspace = 0;
    int schedule_mapping(off_t va, off_t pa, int prot);
    int sort_regions(void);
    off_t find_hole(size_t size);



    // expects nothing to be locked
    ref<mm::page> get_page(off_t uaddr);
    // expects the area, and space to be locked
    ref<mm::page> get_page_internal(off_t uaddr, mm::area &area, int pagefault_err, bool do_map);

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
