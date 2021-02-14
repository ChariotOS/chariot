#include <cpu.h>
#include <lock.h>
#include <mm.h>
#include <phys.h>
mm::space::space(off_t lo, off_t hi, ref<mm::pagetable> pt) : pt(pt), lo(lo), hi(hi) {}

mm::space::~space(void) {
  for (auto &r : regions) {
    delete r;
  }
  // clear out our handle to each area
  regions.clear();
}

void mm::space::switch_to() { pt->switch_to(); }




size_t mm::space::copy_out(off_t byte_offset, void *dst, size_t size) {
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


#if 0
  printk("[%3d] fault at %p err: %02x   ", curproc->pid, va, err);
  if (err & FAULT_WRITE) printk(" write");
  if (err & FAULT_READ) printk(" read");
  if (err & FAULT_EXEC) printk(" exec");
  printk("\n");
#endif

  // TODO:
  // if (err & PGFLT_USER && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_WRITE && !(r->prot & VPROT_WRITE)) fault_res = -1;
  if (err & FAULT_READ && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_EXEC && !(r->prot & VPROT_EXEC)) fault_res = -1;

  if (fault_res == 0) {
    // handle the fault in the region
    auto page = get_page_internal(va, *r, err, true);
    if (!page) return -1;
  }

  return 0;
}


// return the page at an address (allocate if needed)
ref<mm::page> mm::space::get_page(off_t uaddr) {
  scoped_lock l(this->lock);
  auto r = lookup(uaddr);
  if (!r) return nullptr;

  scoped_lock region_lock(r->lock);
  return get_page_internal(uaddr, *r, 0, false);
}




ref<mm::page> mm::space::get_page_internal(off_t uaddr, mm::area &r, int err, bool do_map) {
  struct mm::pte pte;
  pte.prot = r.prot;



  bool display = false; //r.name == "[stack]";
  // the page index within the region
  auto ind = (uaddr >> 12) - (r.va >> 12);

  if (!r.pages[ind]) {
    bool got_from_vmobj = false;
    ref<mm::page> page;
    if (r.obj) {
      page = r.obj->get_shared(ind);
      got_from_vmobj = true;

      // remove the protection so we can detect writes and mark pages as dirty
      // or COW them
      pte.prot &= ~VPROT_WRITE;
    }

    if (!page && got_from_vmobj) {
      panic("failed!\n");
    }

    if (!page) {
      // anonymous mapping
      page = mm::page::alloc();
    } else {
      pte.nocache = page->fcheck(PG_NOCACHE);
      pte.writethrough = page->fcheck(PG_WRTHRU);
    }


    spinlock::lock(page->lock);
    page->users++;
    spinlock::unlock(page->lock);

    r.pages[ind] = page;
  }


  // If the fault was due to a write, and this region
  // is writable, handle COW if needed
  if ((err & FAULT_WRITE) && (r.prot & PROT_WRITE)) {
    // printk(KERN_WARN "[pid=%d] WOW [page %d in '%s'] %p\n", curthd->pid, ind, r.name.get(), uaddr);
    pte.prot = r.prot;

    if (r.flags & MAP_SHARED) {
      // TODO: handle shared
      // printk(KERN_INFO "Write to shared page %p\n", r.pages[ind]->pa);
			r.pages[ind]->fset(PG_DIRTY);
    } else {
      auto old_page = r.pages[ind];
      spinlock::lock(old_page->lock);

      if (old_page->users > 1 || r.fd) {
        auto np = mm::page::alloc();
        if (display) printk(KERN_WARN "[pid=%d] COW [page %d in '%s'] %p\n", curthd->pid, ind, r.name.get(), uaddr);
        np->users = 1;
        old_page->users--;
        memcpy(p2v(np->pa), p2v(old_page->pa), PGSIZE);
        r.pages[ind] = np;
      } else {
        if (display) printk(KERN_WARN "[pid=%d] OWN [page %d in '%s'] %p\n", curthd->pid, ind, r.name.get(), uaddr);
      }

      spinlock::unlock(old_page->lock);
    }
  }

  if (do_map) {
    if (display) printk(KERN_WARN "[pid=%d] map %p to %p\n", curproc->pid, uaddr & ~0xFFF, r.pages[ind]->pa);
    pte.ppn = r.pages[ind]->pa >> 12;
    auto va = (r.va + (ind << 12));
    pt->add_mapping(va, pte);
  }

  // printk(KERN_WARN "[pid=%d]    get_page_internal(%p) = %p\n", curthd->pid, uaddr, r.pages[ind].get());
  return r.pages[ind];
}


size_t mm::space::memory_usage(void) {
  scoped_lock l(lock);

  size_t s = 0;

  for (auto &r : regions) {
    r->lock.lock();
    for (auto &p : r->pages)
      if (p) {
        s += sizeof(mm::page);
        s += PGSIZE;
      }

    s += sizeof(mm::page *) * r->pages.size();
    r->lock.unlock();
  }
  s += sizeof(mm::space);

  return s;
}
#define DO_COW

mm::space *mm::space::fork(void) {
  auto npt = mm::pagetable::create();
  auto *n = new mm::space(lo, hi, npt);

  scoped_lock self_lock(lock);


  for (auto &r : regions) {
    // printk(KERN_WARN "[pid=%d] fork %s\n", curproc->pid, r->name.get());
    auto copy = new mm::area;
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
		// printk("prot: %b, flags: %b\n", r->prot, r->flags);

    int i = 0;
    for (auto &p : r->pages) {
      i++;
      if (p) {
      	// printk(KERN_WARN "[pid=%d]       page %d: %p\n", curproc->pid, i, p->pa);
        spinlock::lock(p->lock);
        p->users++;
        copy->pages.push(p);

        spinlock::unlock(p->lock);
      } else {
        // TODO: manage shared mapping on fork
        // push an empty page to take up the space
        copy->pages.push(nullptr);
      }
    }

    for (size_t i = 0; i < round_up(r->len, 4096) / 4096; i++) {
      struct mm::pte pte;
      if (r->pages[i]) {
        pte.ppn = r->pages[i]->pa >> 12;
        // for copy on write
        pte.prot = r->prot & ~PROT_WRITE;
        pt->add_mapping(r->va + (i * 4096), pte);
        n->pt->add_mapping(r->va + (i * 4096), pte);
      }
    }

    n->regions.push(copy);
  }

  n->sort_regions();

  return n;
}

off_t mm::space::mmap(off_t req, size_t size, int prot, int flags, ref<fs::file> fd, off_t off) {
  return mmap("", req, size, prot, flags, move(fd), off);
}

off_t mm::space::mmap(string name, off_t addr, size_t size, int prot, int flags, ref<fs::file> fd, off_t off) {
  if (addr & 0xFFF) return -1;

  if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0) {
    printk("map without private or shared\n");
    return -1;
  }


  scoped_lock l(lock);

  if (addr == 0) {
    addr = find_hole(round_up(size, 4096));
  }

  off_t pages = round_up(size, 4096) / 4096;

  ref<mm::vmobject> obj = nullptr;

  // if there is a file descriptor, try to call it's mmap. Otherwise fail
  if (fd) {
    if (fd->ino && fd->ino->fops && fd->ino->fops->mmap) {
      obj = fd->ino->fops->mmap(*fd, pages, prot, flags, off);
    }

    if (!obj) {
      return -1;
    }
    obj->acquire();
  }


  auto r = new mm::area();

  r->name = name;
  r->va = addr;
  r->len = pages * 4096;
  r->off = off;
  r->prot = prot;
  r->flags = flags;
  r->fd = fd;
  r->obj = obj;
  r->pages.ensure_capacity(pages);

  for (int i = 0; i < pages; i++) r->pages.push(nullptr);

  regions.push(r);

  sort_regions();

  return addr;
}

int mm::space::unmap(off_t ptr, size_t len) {
  scoped_lock l(lock);

  off_t va = (off_t)ptr;
  if ((va & 0xFFF) != 0) return -1;


  len = round_up(len, 4096);

  for (int i = 0; i < regions.size(); i++) {
    auto region = regions[i];

    if (region->va == va && NPAGES(region->len) == NPAGES(len)) {
      for (off_t v = va; v < va + len; v += 4096) {
        pt->del_mapping(v);
      }


      regions.remove(i);
      sort_regions();
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
      printk(KERN_WARN "validate_pointer(%p) - region not found!\n", raw_va);
      return false;
    }

    if ((mode & PROT_READ && !(r->prot & PROT_READ)) || (mode & PROT_WRITE && !(r->prot & PROT_WRITE)) ||
        (mode & PROT_EXEC && !(r->prot & PROT_EXEC))) {
      printk(KERN_WARN "validate_pointer(%p) - protection!\n", raw_va);
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

void mm::space::dump(void) {
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

