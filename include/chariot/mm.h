#pragma once

#ifndef __MM_H_
#define __MM_H_

#include <fs.h>
#include <mmap_flags.h>
#include <ptr.h>
#include <rbtree.h>
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

#define VALIDATE_RDWR(ptr, size) \
  curproc->mm->validate_pointer((void *)ptr, size, PROT_WRITE | PROT_READ)

#define VALIDATE_EXEC(ptr, size) curproc->mm->validate_pointer((void *)ptr, size, PROT_EXEC)


namespace mm {


  // every physical page in mm circulation is kept track of via a heap-allocated
  // `struct page`.
  struct page : public refcounted<mm::page> {
#define PG_DIRTY (1ul << 0)
#define PG_OWNED (1ul << 1)
#define PG_WRTHRU (1ul << 2)
#define PG_NOCACHE (1ul << 3)
#define PG_BCACHE (1ul << 4)

    inline void fset(int set) { m_paf |= set; }

    inline void fclr(int set) { m_paf &= ~set; }

    inline int flags(void) { return m_paf & 0xFFF; }

    /* return if a certain (set of) flag(s) is set or not */
    inline bool fcheck(int set) { return !((m_paf & set) == 0); }


    page(void);
    ~page(void);

    static ck::ref<page> alloc(void);
    // create a page mapping for some physical memory
    // note: this page isn't owned.
    static ck::ref<page> create(unsigned long pa);

    inline unsigned long pa(void) { return m_paf & ~0xFFF; }

    inline void set_pa(unsigned long pa) { m_paf = (pa & ~0xFFF) | flags(); }

    inline void lock(void) {
      // printk("lock page %p\n", this);
      spinlock::lock(m_lock);
    }
    inline void unlock(void) {
      // printk("unlock page %p\n", this);
      spinlock::unlock(m_lock);
    }

    inline uint32_t users(void) { return __atomic_load_n(&m_users, __ATOMIC_ACQUIRE); }


    struct list_head mappings;
    /**
     * users is a representation of how many holders of this page there are.
     * This is useful for COW mappings, because you must copy the page on write
     * if and only if users > 1 (just you). If any other users exist, you copy
     * it, decrement the users count, and replace your reference with the new
     * one.
     *
     * This field is only used by the mm::page_mapping structure. Any writing
     * outside of that class is illegal and will result in race conditions
     */
    volatile uint32_t m_users = 0;

   private:
    int32_t m_lock = 0;
    unsigned long m_lru = 0;
    /* Physical address and the flags stored in the lower 12 bits */
    unsigned long m_paf = 0;
  };

  class page_mapping {
   public:
    inline page_mapping(ref<mm::page> pg) { set_page(pg); }

    inline page_mapping(void) { set_page(nullptr); }

    inline page_mapping(nullptr_t v) { set_page(v); }

    // expects pg's lock to be held
    inline void set_page(ref<mm::page> pg) {
      if (!is_null()) {
        // decrement users
        __atomic_sub_fetch(&page->m_users, 1, __ATOMIC_ACQ_REL);
        page = nullptr;
      }

      if (!pg.is_null()) {
        __atomic_add_fetch(&pg->m_users, 1, __ATOMIC_ACQ_REL);
        page = pg;
      }
    }

    inline bool is_null(void) { return page.is_null(); }
    inline operator bool(void) { return !is_null(); }


    inline auto *operator->(void) { return page.get(); }
    inline auto get(void) { return page; }


    inline page_mapping &operator=(ref<mm::page> pg) {
      // printk("operator= %p\n", pg.get());
      set_page(pg);
      return *this;
    }

   protected:
    friend mm::page;

    struct list_head mappings;
    ck::ref<mm::page> page;
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
    static ck::ref<pagetable> create();
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
    virtual ck::ref<mm::page> get_shared(off_t n) = 0;
    // tell the region to flush a page
    virtual void flush(off_t n) {}

    inline size_t size(void) { return n_pages * PGSIZE; }

   private:
    spinlock m_lock;
    int owners = 0;
    size_t n_pages;
  };


  struct area {
    string name;

    off_t va;
    size_t len;  // in bytes
    off_t off;   // offset into the mapped file

    // flags from mmap (MAP_SHARED and co)
    int prot = 0;
    int flags = 0;

    spinlock lock;

    // TODO: unify shared mappings in the fileriptor somehow
    ck::ref<fs::file> fd;
    ck::vec<mm::page_mapping> mappings;  // backing memory

    // optional. If it exists, it is queried for each page
    // This is required if the region is not anonymous. If a region is mapped
    // and it isn't anon but has no obj field, it acts like an anon mapping
    ck::ref<vmobject> obj = nullptr;

    /* The entry in the rbtree */
    rb_node node;


    area(void);
    ~area(void);


    static inline int compare(area &a, area &b) { return a.va - a.va; }
  };

  class space {
   public:
    ck::ref<mm::pagetable> pt;

    off_t lo, hi;

    space(off_t lo, off_t hi, ck::ref<mm::pagetable>);
    ~space(void);


    size_t copy_out(off_t addr, void *into, size_t len);

    inline auto get_pt(void) { return pt; }
    void switch_to();
    mm::area *lookup(off_t va);

    int delete_region(off_t va);
    int pagefault(off_t va, int err);
    off_t mmap(off_t req, size_t size, int prot, int flags, ck::ref<fs::file>, off_t off);

    off_t mmap(
        string name, off_t req, size_t size, int prot, int flags, ck::ref<fs::file>, off_t off);
    int unmap(off_t addr, size_t sz);


    /* Add a region to the appropriate location in the rbtree */
    bool add_region(mm::area *region);

    // returns the number of bytes resident
    size_t memory_usage(void);

    mm::space *fork(void);

#define VALIDATE_READ 1
#define VALIDATE_WRITE 2
    bool validate_pointer(void *, size_t, int mode);
    bool validate_string(const char *);


    template <typename T>
    inline bool validate_null_terminated(const T *str) {
      /* Safe first guess :^) */
      if (str == NULL) return false;
      /* Validate the first byte */
      if (!validate_pointer((void *)str, 1, VALIDATE_READ)) return false;

      // the current virtual page number that we know is valid
      off_t valid_vpn = (off_t)str >> 12;

      // TODO: protect against other threads remapping memory under the string
      int len = 0;
      for (len = 0; true; len++) {
        const T *cur = str + len;
        off_t current_vpn = (off_t)cur >> 12;
        if (current_vpn != valid_vpn) {
          /* If it's not valid, and we have not hit a \0, the string is not valid */
          if (!validate_pointer((void *)cur, 1, VALIDATE_READ)) return false;
          /* We have validate the current_vpn */
          valid_vpn = current_vpn;
        }
        /* Since we have validated the pointer, we can now check the byte */
        if (*cur == 0) break;
      }

      return true;
    }

    template <typename T, typename V>
    bool validate_struct(V val, int mode) {
      return validate_pointer((void *)(val), sizeof(T), mode);
    }

    // impl by arch::
    static mm::space &kernel_space(void);

    void dump();

    int is_kspace = 0;
    int schedule_mapping(off_t va, off_t pa, int prot);
    off_t find_hole(size_t size);



    // expects nothing to be locked
    ck::ref<mm::page> get_page(off_t uaddr);
    // expects the area, and space to be locked
    ck::ref<mm::page> get_page_internal(
        off_t uaddr, mm::area &area, int pagefault_err, bool do_map);

    spinlock lock;
    rb_root regions;
    // ck::vec<mm::area *> regions;

    struct pending_mapping {
      off_t va;
      off_t pa;
      int prot;
    };

    unsigned long revision;
    unsigned long kmem_revision;
    ck::vec<pending_mapping> pending_mappings;
  };
};  // namespace mm

#endif
