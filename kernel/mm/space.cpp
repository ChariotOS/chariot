#include <cpu.h>
#include <errno.h>
#include <lock.h>
#include <mm.h>
#include <phys.h>
#include <syscall.h>
#include <time.h>

mm::AddressSpace::AddressSpace(off_t lo, off_t hi, ck::ref<mm::PageTable> pt) : pt(pt), lo(lo), hi(hi) {}


mm::AddressSpace::~AddressSpace(void) {
  // printf("mm: faults: %lu, hits: %lu, misses: %lu\n", pagefaults, predict_hits, predict_misses);
  mm::MappedRegion *n, *node;
  rbtree_postorder_for_each_entry_safe(node, n, &regions, node) { delete node; }
}

void mm::AddressSpace::switch_to() { pt->switch_to(); }



bool mm::AddressSpace::add_region(mm::MappedRegion *region) {
  RegionCacheEntry *lru_cache_entry = NULL;
  // check the cache first...
  for (int i = 0; i < MM_REGION_CACHE_SIZE; i++) {
    auto &rce = region_cache[i];
    if (lru_cache_entry == NULL || lru_cache_entry->last_used > rce.last_used) {
      lru_cache_entry = &rce;
    }
  }

  if (lru_cache_entry) {
    lru_cache_entry->region = region;
    lru_cache_entry->last_used = cache_tick++;
  }

  return rb_insert(regions, &region->node, [&](struct rb_node *other) {
    auto *other_region = rb_entry(other, struct mm::MappedRegion, node);
    long result = (long)region->va - (long)other_region->va;

    if (result < 0)
      return RB_INSERT_GO_LEFT;
    else if (result > 0)
      return RB_INSERT_GO_RIGHT;
    else {
      return RB_INSERT_GO_HERE;
    }
  });
}


size_t mm::AddressSpace::copy_out(off_t byte_offset, void *dst, size_t size) {
  // how many more bytes are needed
  long to_access = size;
  // the offset within the current page
  ssize_t offset = byte_offset % PGSIZE;

  char *udata = (char *)dst;
  size_t read = 0;

  for (off_t blk = byte_offset / PGSIZE; true; blk++) {
    struct pte pte;
    pt->get_mapping(blk * PGSIZE, pte);

    if (pte.ppn == 0) break;
    // get the block we are looking at.
    auto data = (char *)(p2v(pte.ppn << 12));

    size_t space_left = PGSIZE - offset;
    size_t can_access = min(space_left, to_access);

    // copy the data from the page
    memcpy(udata, data + offset, can_access);

    // moving on to the next block, we reset the offset
    offset = 0;
    to_access -= can_access;
    read += can_access;
    udata += can_access;

    if (to_access <= 0) break;
  }

  return read;
}


ck::vec<mm::MappedRegion *> mm::AddressSpace::lookup_range(off_t va, size_t sz) {
  ck::vec<mm::MappedRegion *> in_range;

  //
  struct rb_node **n = &(regions.rb_node);
  struct rb_node *parent = NULL;

  int steps = 0;

  off_t start = va;
  off_t end = va + sz;

  /* Figure out where to put new node */
  while (*n != NULL) {
    auto *r = rb_entry(*n, struct mm::MappedRegion, node);

    auto region_start = r->va;
    auto region_end = r->va + r->len;
    parent = *n;
    steps++;


    bool start_inside = start >= region_start && start < region_end;
    bool end_inside = end > region_start && end <= region_end;
    bool region_inside = start <= region_start && region_end < end;

    if (start_inside || end_inside || region_inside) {
      in_range.push(r);
    }

    if (va < region_start) {
      n = &((*n)->rb_left);
    } else if (va >= region_end) {
      n = &((*n)->rb_right);
    } else {
      break;
    }
  }

  return in_range;
}

mm::MappedRegion *mm::AddressSpace::lookup(off_t va) {
  RegionCacheEntry *lru_cache_entry = NULL;
  // check the cache first...
  for (int i = 0; i < MM_REGION_CACHE_SIZE; i++) {
    auto &rce = region_cache[i];
    if (lru_cache_entry == NULL || lru_cache_entry->last_used > rce.last_used) {
      lru_cache_entry = &rce;
    }

    if (rce.region) {
      if (rce.region->va <= va && va < rce.region->va + rce.region->len) {
        rce.last_used = cache_tick++;
        cache_hits++;
        return rce.region;
      }
    }
  }
  cache_misses++;


  struct rb_node **n = &(regions.rb_node);
  struct rb_node *parent = NULL;

  int steps = 0;

  /* Figure out where to put new node */
  while (*n != NULL) {
    auto *r = rb_entry(*n, struct mm::MappedRegion, node);

    auto start = r->va;
    auto end = r->va + r->len;
    parent = *n;

    steps++;

    if (va < start) {
      n = &((*n)->rb_left);
    } else if (va >= end) {
      n = &((*n)->rb_right);
    } else {
      if (lru_cache_entry != nullptr) {
        lru_cache_entry->region = r;
        lru_cache_entry->last_used = cache_tick++;
      }
      return r;
    }
  }

  return NULL;
}


int mm::AddressSpace::delete_region(off_t va) { return -1; }


struct xcall_flush {
  mm::AddressSpace *space;
  off_t va;      // the start virtual address
  size_t pages;  // how many pages?
};




static void xcall_flush(mm::AddressSpace *space, off_t va, size_t pages) {
  struct xcall_flush arg;
  arg.space = space;
  arg.va = va;
  arg.pages = pages;
  cpu::xcall_all(
      [](void *v) {
        auto *arg = (struct xcall_flush *)v;
        if (curproc) {
          if (curproc->mm == arg->space) {
            arch_flush_mmu();
          }
        }
      },
      &arg);
}



int mm::AddressSpace::pagefault(off_t va, int err) {
  scoped_lock l(this->lock);
	pagefaults++;
  va &= ~0xFFF;
  auto r = lookup(va);

  if (!r) return -1;

  scoped_lock region_lock(r->lock);

  int fault_res = 0;

  // TODO: USER access fault
  // if (err & PGFLT_USER && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_WRITE && !(r->prot & VPROT_WRITE)) fault_res = -1;
  if (err & FAULT_READ && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_EXEC && !(r->prot & VPROT_EXEC)) fault_res = -1;

  if (fault_res == 0) {
    // handle the fault in the region
    pt->transaction_begin("pflt");
    auto page = get_page_internal(va, *r, err, true);

#if CONFIG_MEMORY_PREFETCH
    predict_i = min(predict_i + 1, PREDICT_I_MAX);

    if (va == 0 || va != predict_next) {
      predict_i = 0;
			predict_misses++;
    } else {
			predict_hits++;
		}

    off_t skip = (1 << predict_i) - 1;
    if (page && skip > 0) {
      for (int i = 0; i < skip; i++) {
        off_t addr = va + (i + 1) * PGSIZE;
        if (get_page_internal(addr, *r, FAULT_READ, true).is_null()) {
          break;
        }
      }
    }
    // Predict that the next fault will be caused by predict_next
    predict_next = va + ((1 << predict_i) * PGSIZE);
#endif
    pt->transaction_commit();
    if (!page) return -1;
  }

  // xcall_flush(this, va, 1);

  // printf_nolock("pgfault: %dpfltu, %llu\n", curthd->tid, va, start, arch_read_timestamp() - start);
  return 0;
}


// return the page at an address (allocate if needed)
ck::ref<mm::Page> mm::AddressSpace::get_page(off_t uaddr) {
  scoped_lock l(this->lock);
  auto r = lookup(uaddr);
  if (!r) {
    return nullptr;
  }


  r->lock.lock();
  pt->transaction_begin("get_page");
  auto pg = get_page_internal(uaddr, *r, 0, true);
  pt->transaction_commit();
  r->lock.unlock();
  return pg;
}




ck::ref<mm::Page> mm::AddressSpace::get_page_internal(off_t uaddr, mm::MappedRegion &r, int err, bool do_map) {
  struct mm::pte pte;
  pte.prot = r.prot;


  // if the page was allocated as a fresh anon page, don't cause a COW fault later
  bool maybe_shared = true;
  bool display = false;

  // the page index within the region
  auto ind = (uaddr >> 12) - (r.va >> 12);
  if (ind >= r.mappings.size()) return nullptr;

  if (r.mappings[ind].is_null()) {
    bool got_from_vmobj = false;
    ck::ref<mm::Page> page = nullptr;
    if (r.obj) {
      page = r.obj->get_shared(ind);
      got_from_vmobj = true;
      // remove the protection so we can detect writes and mark pages as dirty
      // or COW them
      pte.prot &= ~VPROT_WRITE;
    }

    if (!page && got_from_vmobj) panic("failed!\n");

    // anonymous mapping
    if (!page) {
      page = mm::Page::alloc();
      maybe_shared = false;
    }

    pte.nocache = page->fcheck(PG_NOCACHE);
    pte.writethrough = page->fcheck(PG_WRTHRU);

    r.mappings[ind].set_page(page);
  }

  auto page = r.mappings[ind];


  // If the fault was due to a write, and this region
  // is writable, handle COW if needed
  if (maybe_shared && (err & FAULT_WRITE) && (r.prot & PROT_WRITE)) {
    pte.prot |= PROT_WRITE;

    // a shared mapping
    if (r.flags & MAP_SHARED) {
      page->fset(PG_DIRTY);
    }

    // a private mapping must be copied if there are two users
    if (r.flags & MAP_PRIVATE) {
      auto old_page = page;
      old_page->lock();

      if (old_page->users() > 1 || r.fd) {
        auto np = mm::Page::alloc();
        // no need to take the new page's lock here, it's only referenced here.
        if (display) printf(KERN_WARN "[pid=%d] COW [page %d in '%s'] %p\n", curthd->pid, ind, r.name.get(), uaddr);
        memcpy(p2v(np->pa()), p2v(old_page->pa()), PGSIZE);
        r.mappings[ind] = np;
        page = np;
      }

      old_page->unlock();
    }
  }


  if (do_map) {
    if (display) printf(KERN_WARN "[pid=%d] map %p to %p\n", curproc->pid, uaddr & ~0xFFF, r.mappings[ind]->pa());
    pte.ppn = page->pa() >> 12;
    auto va = (r.va + (ind << 12));
    pt->add_mapping(va, pte);
  } else {
    printf("DONT MAP\n");
  }

  return page.get();
}


size_t mm::AddressSpace::memory_usage(void) {
  scoped_lock l(lock);

  size_t s = 0;

  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::MappedRegion, node);
    r->lock.lock();
    for (auto &p : r->mappings)
      if (p) {
        s += sizeof(mm::Page);
        s += PGSIZE;
      }

    s += sizeof(mm::Page *) * r->mappings.size();
    r->lock.unlock();
  }
  s += sizeof(mm::AddressSpace);

  return s;
}



unsigned long sys::getramusage() { return curproc->mm->memory_usage(); }


mm::AddressSpace *mm::AddressSpace::fork(void) {
  auto npt = mm::PageTable::create();
  auto *n = new mm::AddressSpace(lo, hi, npt);

  pt->transaction_begin("fork source");
  npt->transaction_begin("fork target");

  scoped_lock self_lock(lock);


  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::MappedRegion, node);
    // printf(KERN_WARN "[pid=%d] fork %s\n", curproc->pid, r->name.get());
    auto copy = new mm::MappedRegion;
    copy->name = r->name;
    copy->va = r->va;
    copy->len = r->len;
    copy->off = r->off;
    copy->prot = r->prot;
    copy->fd = r->fd;
    copy->flags = r->flags;

    if (r->obj) {
      copy->obj = r->obj;
      r->obj->acquire();
    }
    // printf("prot: %b, flags: %b\n", r->prot, r->flags);

    int i = 0;
    for (auto &m : r->mappings) {
      i++;
      if (m) {
        copy->mappings.push(m.get());
      } else {
        // TODO: manage shared mapping on fork
        // push an empty page to take up the space
        copy->mappings.push(nullptr);
      }
    }

    for (size_t i = 0; i < round_up(r->len, 4096) / 4096; i++) {
      struct mm::pte pte;
      if (r->mappings[i]) {
        pte.ppn = r->mappings[i]->pa() >> 12;
        // for copy on write
        pte.prot = r->prot & ~PROT_WRITE;
        pt->add_mapping(r->va + (i * 4096), pte);
        n->pt->add_mapping(r->va + (i * 4096), pte);
      }
    }

    n->add_region(copy);
  }

  npt->transaction_commit();
  pt->transaction_commit();


  return n;
}

off_t mm::AddressSpace::mmap(off_t req, size_t size, int prot, int flags, ck::ref<fs::File> fd, off_t off) {
  return mmap("", req, size, prot, flags, move(fd), off);
}

off_t mm::AddressSpace::mmap(ck::string name, off_t addr, size_t size, int prot, int flags, ck::ref<fs::File> fd, off_t off) {
  if (addr & 0xFFF) return -1;

  if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0) {
    printf("map without private or shared\n");
    return -1;
  }


  scoped_lock l(lock);

  if (addr == 0) {
    addr = find_hole(round_up(size, 4096));
  } else {
    //
  }

  off_t pages = round_up(size, 4096) / 4096;

  ck::ref<mm::VMObject> obj = nullptr;

  // if there is a file descriptor, try to call it's mmap. Otherwise fail
  if (fd) {
    obj = fd->ino->mmap(*fd, pages, prot, flags, off);

    if (!obj) {
      return -1;
    }
    obj->acquire();
  }


  auto r = new mm::MappedRegion();

  r->name = name;
  r->va = addr;
  r->len = pages * 4096;
  r->off = off;
  r->prot = prot;
  r->flags = flags;
  r->fd = fd;
  r->obj = obj;
  r->mappings.ensure_capacity(pages);

  for (int i = 0; i < pages; i++)
    r->mappings.push(nullptr);


  add_region(r);

  xcall_flush(this, addr, pages);

  return addr;
}

int mm::AddressSpace::unmap(off_t ptr, size_t ulen) {
  off_t va = (off_t)ptr;
  if ((va & 0xFFF) != 0) return -1;


  size_t len = round_up(ulen, 4096);

  scoped_irqlock l1(lock);
  auto *region = lookup(va);
  if (region == NULL) return -ESRCH;

  scoped_irqlock l2(region->lock);

  for (int i = 0; i < MM_REGION_CACHE_SIZE; i++) {
    if (region_cache[i].region == region) {
      region_cache[i].region = nullptr;
      region_cache[i].last_used = 0;
    }
  }

  rb_erase(&region->node, &regions);
  pt->transaction_begin();
  for (int i = 0; i < region->mappings.size(); i++) {
    off_t addr = va + (i * 4096);
    if (!region->mappings[i].is_null()) {
      pt->del_mapping(addr);
    }
  }
  pt->transaction_commit();

  delete region;

  return 0;
}

#define PGMASK (~(PGSIZE - 1))
bool mm::AddressSpace::validate_pointer(void *raw_va, size_t len, int mode) {
  if (is_kspace) return true;
  scoped_lock l(this->lock);
  off_t start = (off_t)raw_va & PGMASK;
  off_t end = ((off_t)raw_va + len) & PGMASK;

  for (off_t va = start; va <= end; va += PGSIZE) {
    // see if there is a region at the requested offset
    auto r = lookup(va);
    if (!r) {
      // printf(KERN_WARN "validate_pointer(%p) - region not found!\n", raw_va);
      // this->dump();
      return false;
    }

    if ((mode & PROT_READ && !(r->prot & PROT_READ)) || (mode & PROT_WRITE && !(r->prot & PROT_WRITE)) ||
        (mode & PROT_EXEC && !(r->prot & PROT_EXEC))) {
      printf(KERN_WARN "validate_pointer(%p) - protection!\n", raw_va);
      return false;
    }
    // TODO: check mode flags
  }
  return true;
}

bool mm::AddressSpace::validate_string(const char *str) { return validate_null_terminated(str); }


void mm::AddressSpace::dump(void) {
  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::MappedRegion, node);
    printf("%p-%p ", r->va, r->va + r->len);
    printf("%6zupgs", r->mappings.size());
    int mapped = 0;
    for (int i = 0; i < r->mappings.size(); i++)
      if (!r->mappings[i].is_null()) mapped++;
    printf(" %3d%%", (mapped * 100) / r->mappings.size());

    printf("");
    printf("%c", r->prot & VPROT_READ ? 'r' : '-');
    printf("%c", r->prot & VPROT_WRITE ? 'w' : '-');
    printf("%c", r->prot & VPROT_EXEC ? 'x' : '-');
    printf(" %08lx", r->off);
    int maj = 0;
    int min = 0;
    if (r->fd) {
      // maj = r->fd->ino->dev.major;
      // min = r->fd->ino->dev.minor;
    }


    printf(" %02x:%02x", maj, min);
    // printf(" %-10d", r->fd ? r->fd->ino->ino : 0);
    printf(" %s", r->name.get());

    printf("\n");
  }
  printf("\n");
}

off_t mm::AddressSpace::find_hole(size_t size) {
#ifdef CONFIG_TOP_DOWN
  off_t va = this->hi - size;
  off_t lim = va + size;

  for (struct rb_node *node = rb_last(&regions); node; node = rb_prev(node)) {
    auto *r = rb_entry(node, struct mm::MappedRegion, node);

    auto rva = r->va;
    auto rlim = rva + r->len;

    if (va <= rlim && rva < lim) {
      va = rva - size;
      lim = va + size;
    } else {
      break;
    }
  }
  return va;

#else  // BOTTOM UP

  off_t va = this->lo;
  off_t lim = va + size;

  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::MappedRegion, node);

    auto rva = r->va;
    auto rlim = rva + r->len;

    if (va <= rlim && rva < lim) {
      va = rlim;
      lim = va + size;
    } else {
      break;
    }
  }

  return va;
#endif
}
