#include <mm.h>
#include <phys.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

mm::page::~page(void) {
  if (owns_page) {
    phys::free((void *)pa);
  }
  assert(users == 0);
  pa = 0;
}

ref<mm::page> mm::page::alloc(void) {
  auto p = make_ref<mm::page>();
  p->pa = (u64)phys::alloc();
  p->users = 0;
  p->owns_page = 1;
  return move(p);
}

mm::space::space(off_t lo, off_t hi, ref<mm::pagetable> pt)
    : pt(pt), lo(lo), hi(hi) {}

mm::space::~space(void) {
  //
  for (auto r : regions) {
    delete r;
  }
}

void mm::space::switch_to() { pt->switch_to(); }

mm::area *mm::space::lookup(off_t va) {
  // just dumbly walk over the list of regions and find the right region
  for (auto &r : regions) {
    off_t start = r->va;

    if (va >= start && va < start + r->len) {
      return r;
    }
  }

  return nullptr;
}

int mm::space::delete_region(off_t va) { return -1; }

int mm::space::pagefault(off_t va, int err) {
  scoped_lock l(this->lock);

  auto r = lookup(va);

  if (!r) return -1;

  scoped_lock region_lock(r->lock);

  int fault_res = 0;

  // TODO:
  // if (err & PGFLT_USER && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_WRITE && !(r->prot & VPROT_WRITE)) fault_res = -1;
  if (err & FAULT_READ && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_EXEC && !(r->prot & VPROT_EXEC)) fault_res = -1;

  // the page index within the region
  auto ind = (va >> 12) - (r->va >> 12);
  struct mm::pte pte;
  pte.prot = r->prot;

  if (fault_res == 0) {
    // handle the fault in the region
    if (!r->pages[ind]) {
      r->pages[ind] = mm::page::alloc();
      auto &page = r->pages[ind];
      spinlock::lock(page->lock);
      page->users++;

      /**
       * if the region is backed by a file, read the file into the page
       */
      if (r->fd) {
        void *buf = p2v(page->pa);
        // TODO: handle failures to read

        // offset, in bytes, into the region
        off_t roff = ind * PGSIZE;
        r->fd->seek(r->off + roff, SEEK_SET);

        size_t to_read = min(PGSIZE, r->len - roff);
        int nread = r->fd->read(buf, to_read);

        if (nread == -1) {
          panic("failed to read mapped file!\n");
        }

        if (nread != PGSIZE) {
          // clear out all the other memory that was read
          memset((char *)buf + nread, 0x00, PGSIZE - nread);
        }
      }

      spinlock::unlock(page->lock);
    } else {
      // If the fault was due to a write, and this region
      // is writable, handle COW if needed
      if ((err & FAULT_WRITE) && (r->prot & PROT_WRITE)) {
        if (r->flags & MAP_SHARED) {
          // TODO: handle shared
        } else {
          auto op = r->pages[ind];
          spinlock::lock(op->lock);

          if (op->users > 1) {
            auto np = mm::page::alloc();
            printk("COW [page %d in '%s']\n", ind, r->name.get());
            np->users = 1;
            op->users--;
            memcpy(p2v(np->pa), p2v(op->pa), PGSIZE);
            r->pages[ind] = np;
          }

          spinlock::unlock(op->lock);
        }
      }
    }

    pte.ppn = r->pages[ind]->pa >> 12;
    auto va = (r->va + (ind << 12));
    pt->add_mapping(va, pte);
  }

  return 0;
}

size_t mm::space::memory_usage(void) {
  scoped_lock l(lock);

  size_t s = 0;

  for (auto &r : regions) {
    r->lock.lock();
    for (auto &p : r->pages)
      if (p) s += PGSIZE;
    r->lock.unlock();
  }

  return s;
}

mm::space *mm::space::fork(void) {
  auto npt = mm::pagetable::create();
  auto *n = new mm::space(lo, hi, npt);

  scoped_lock self_lock(this->lock);

  /*
  for (auto &r : regions) {
    printk("%p-%p ", r->va, r->va + r->len);
    printk("%c", r->prot & VPROT_READ ? 'r' : '-');
    printk("%c", r->prot & VPROT_WRITE ? 'w' : '-');
    printk("%c", r->prot & VPROT_EXEC ? 'x' : '-');
    printk(" %08lx", r->off);
    int maj = 0;
    int min = 0;
    if (r->fd) {
      maj = r->fd->ino->dev.major;
      min = r->fd->ino->dev.minor;
    }
    printk(" %02x:%02x", maj, min);
    printk(" %-10d", r->fd ? r->fd->ino->ino : 0);
    printk(" %s", r->name.get());
    printk("\n");
  }
  printk("\n");
  */
#define DO_COW


  for (auto &r : regions) {
    auto copy = new mm::area;
    copy->name = r->name;
    copy->va = r->va;
    copy->len = r->len;
    copy->off = r->off;
    copy->prot = r->prot;
    copy->fd = r->fd;

    for (auto &p : r->pages) {
      if (p) {
        spinlock::lock(p->lock);
#ifdef DO_COW
        p->users++;
        copy->pages.push(p);
#else
        auto np = mm::page::alloc();
        memcpy(p2v(np->pa), p2v(p->pa), PGSIZE);
        copy->pages.push(p);
#endif

        spinlock::unlock(p->lock);
      } else {
        // TODO: manage shared mapping on fork
        // push an empty page to take up the space
        copy->pages.push(nullptr);
      }
    }

#ifdef DO_COW
    for (size_t i = 0; i < round_up(r->len, 4096) / 4096; i++) {
      struct mm::pte pte;
      if (r->pages[i]) {
        pte.ppn = r->pages[i]->pa >> 12;
        // for copy on write
        pte.prot = r->prot & ~PROT_WRITE;
        pt->add_mapping(r->va + (i * 4096), pte);
      }
    }
#endif
    n->regions.push(copy);
  }

  n->sort_regions();

  return n;

  // fail:
  delete n;
  return NULL;
}

off_t mm::space::mmap(off_t req, size_t size, int prot, int flags,
                      ref<fs::file> fd, off_t off) {
  return mmap("[anon]", req, size, prot, flags, move(fd), off);
}

off_t mm::space::mmap(string name, off_t addr, size_t size, int prot, int flags,
                      ref<fs::file> fd, off_t off) {
  if (addr & 0xFFF) return -1;

  scoped_lock l(lock);

  if (addr == 0) {
    addr = find_hole(round_up(size, 4096));
  }

  off_t pages = round_up(size, 4096) / 4096;

  auto r = new mm::area;

  r->name = name;
  r->va = addr;
  r->len = size;
  r->off = off;
  r->prot = prot;
  r->flags = flags;
  r->fd = fd;
  for (int i = 0; i < pages; i++) r->pages.push(nullptr);

  regions.push(r);

  sort_regions();

  return addr;

  return addr;
}

int mm::space::unmap(off_t ptr, size_t len) {
  scoped_lock l(lock);

  off_t va = (off_t)ptr;
  if ((va & 0xFFF) != 0) return -1;

  for (int i = 0; i < regions.size(); i++) {
    auto *region = regions[i];
    if (region->va == va && region->len == len) {
      // printk("removing region %d. Named '%s'\n", i, region->name.get());

      regions.remove(i);
      sort_regions();

      for (off_t v = va; v < va + len; v += 4096) {
        pt->del_mapping(va);
      }

      delete region;
      return 0;
    }
  }

  return -1;
}

#define PGMASK (~(PGSIZE - 1))
bool mm::space::validate_pointer(void *raw_va, size_t len, int mode) {
  if (is_kspace) return true;
  off_t start = (off_t)raw_va & PGMASK;
  off_t end = ((off_t)raw_va + len) & PGMASK;

  for (off_t va = start; va <= end; va += PGSIZE) {
    // see if there is a region at the requested offset
    auto r = lookup(va);
    if (!r) {
      return false;
    }
    // TODO: check mode flags
  }
  return true;
}

bool mm::space::validate_string(const char *str) {
  // TODO: this is really unsafe
  if (validate_pointer((void *)str, 1, VALIDATE_READ)) {
    int len = 0;
    for (len = 0; str[len] != '\0'; len++)
      ;
    return validate_pointer((void *)str, len, VALIDATE_READ);
  }

  return false;
}

int mm::space::schedule_mapping(off_t va, off_t pa, int prot) {
  pending_mappings.push({.va = va, .pa = pa, .prot = prot});
  return 0;
}

int mm::space::sort_regions(void) {
  int const len = regions.size();
  if (len == 1) return 0;
  int n = 0;
  for (int i = 1; i < len; i++) {
    for (int j = i; j > 0; j--) {
      auto &t1 = regions.at(j - 1);
      auto &t2 = regions.at(j);
      if (t1->va > t2->va) {
        swap(t1, t2);
        n++;
      } else {
        break;
      }
    }
  }

  return n;
}

off_t mm::space::find_hole(size_t size) {
  off_t va = hi - size;
  off_t lim = va + size;

  for (int i = regions.size() - 1; i >= 0; i--) {
    auto rva = regions[i]->va;
    auto rlim = rva + regions[i]->len;

    if (va <= rlim && rva < lim) {
      va = rva - size;
      lim = va + size;
    }
  }

  return va;
}

mm::area::~area(void) {
  for (auto &p : pages) {
    if (p) {
      spinlock::lock(p->lock);
      p->users--;
      spinlock::unlock(p->lock);
    }
  }

  pages.clear();
}
