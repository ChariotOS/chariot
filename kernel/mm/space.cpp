#include <cpu.h>
#include <errno.h>
#include <lock.h>
#include <mm.h>
#include <phys.h>


mm::space::space(off_t lo, off_t hi, ck::ref<mm::pagetable> pt) : pt(pt), lo(lo), hi(hi) {}


mm::space::~space(void) {
  mm::area *n, *node;
  rbtree_postorder_for_each_entry_safe(node, n, &regions, node) { delete node; }
}

void mm::space::switch_to() { pt->switch_to(); }



bool mm::space::add_region(mm::area *region) {
  return rb_insert(regions, &region->node, [&](struct rb_node *other) {
    auto *other_region = rb_entry(other, struct mm::area, node);
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
  struct rb_node **n = &(regions.rb_node);
  struct rb_node *parent = NULL;

  int steps = 0;

  /* Figure out where to put new node */
  while (*n != NULL) {
    auto *r = rb_entry(*n, struct mm::area, node);

    auto start = r->va;
    auto end = r->va + r->len;
    parent = *n;

    steps++;

    if (va < start) {
      n = &((*n)->rb_left);
    } else if (va >= end) {
      n = &((*n)->rb_right);
    } else {
      // printk("va: %p, found in %d steps\n", va, steps);
      return r;
    }
  }

  return NULL;
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

  if (fault_res == 0) {
    // handle the fault in the region
    auto page = get_page_internal(va, *r, err, true);
    if (!page) return -1;
  }

  return 0;
}


// return the page at an address (allocate if needed)
ck::ref<mm::page> mm::space::get_page(off_t uaddr) {
  scoped_lock l(this->lock);
  auto r = lookup(uaddr);
  if (!r) {
    return nullptr;
  }


  r->lock.lock();
  auto pg = get_page_internal(uaddr, *r, 0, false);
  r->lock.unlock();
  return pg;
}




ck::ref<mm::page> mm::space::get_page_internal(off_t uaddr, mm::area &r, int err, bool do_map) {
  struct mm::pte pte;
  pte.prot = r.prot;

  bool display = false;

  // the page index within the region
  auto ind = (uaddr >> 12) - (r.va >> 12);

  if (r.mappings[ind].is_null()) {
    bool got_from_vmobj = false;
    ck::ref<mm::page> page = nullptr;
    if (r.obj) {
      page = r.obj->get_shared(ind);
      got_from_vmobj = true;
      // remove the protection so we can detect writes and mark pages as dirty
      // or COW them
      pte.prot &= ~VPROT_WRITE;
    }

    if (!page && got_from_vmobj) panic("failed!\n");

    // anonymous mapping
    if (!page) page = mm::page::alloc();

    pte.nocache = page->fcheck(PG_NOCACHE);
    pte.writethrough = page->fcheck(PG_WRTHRU);

    r.mappings[ind].set_page(page);
  }


  // If the fault was due to a write, and this region
  // is writable, handle COW if needed
  if ((err & FAULT_WRITE) && (r.prot & PROT_WRITE)) {
    pte.prot |= PROT_WRITE;

    // a shared mapping
    if (r.flags & MAP_SHARED) {
      r.mappings[ind]->fset(PG_DIRTY);
    }

    // a private mapping must be copied if there are two users
    if (r.flags & MAP_PRIVATE) {
      auto old_page = r.mappings[ind].get();
      old_page->lock();

      if (old_page->users() > 1 || r.fd) {
        auto np = mm::page::alloc();
        // no need to take the new page's lock here, it's only referenced here.
        if (display)
          printk(KERN_WARN "[pid=%d] COW [page %d in '%s'] %p\n", curthd->pid, ind, r.name.get(),
              uaddr);
        memcpy(p2v(np->pa()), p2v(old_page->pa()), PGSIZE);
        r.mappings[ind] = np;
      }

      old_page->unlock();
    }
  }


  if (do_map) {
    if (display)
      printk(
          KERN_WARN "[pid=%d] map %p to %p\n", curproc->pid, uaddr & ~0xFFF, r.mappings[ind]->pa());
    pte.ppn = r.mappings[ind]->pa() >> 12;
    auto va = (r.va + (ind << 12));
    pt->add_mapping(va, pte);
  }

  return r.mappings[ind].get();
}


size_t mm::space::memory_usage(void) {
  scoped_lock l(lock);

  size_t s = 0;

  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::area, node);
    r->lock.lock();
    for (auto &p : r->mappings)
      if (p) {
        s += sizeof(mm::page);
        s += PGSIZE;
      }

    s += sizeof(mm::page *) * r->mappings.size();
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


  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::area, node);
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


  return n;
}

off_t mm::space::mmap(
    off_t req, size_t size, int prot, int flags, ck::ref<fs::file> fd, off_t off) {
  return mmap("", req, size, prot, flags, move(fd), off);
}

off_t mm::space::mmap(ck::string name, off_t addr, size_t size, int prot, int flags,
    ck::ref<fs::file> fd, off_t off) {
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

  ck::ref<mm::vmobject> obj = nullptr;

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
  r->mappings.ensure_capacity(pages);

  for (int i = 0; i < pages; i++)
    r->mappings.push(nullptr);


  add_region(r);

  return addr;
}

int mm::space::unmap(off_t ptr, size_t ulen) {
  off_t va = (off_t)ptr;
  if ((va & 0xFFF) != 0) return -1;


  size_t len = round_up(ulen, 4096);

  scoped_irqlock l1(lock);
  auto *region = lookup(va);
  if (region == NULL) return -ESRCH;

  scoped_irqlock l2(region->lock);

  rb_erase(&region->node, &regions);

  for (int i = 0; i < region->mappings.size(); i++) {
    off_t addr = va + (i * 4096);
    if (!region->mappings[i].is_null()) {
      pt->del_mapping(addr);
    }
  }

  delete region;

  return 0;
}

#define PGMASK (~(PGSIZE - 1))
bool mm::space::validate_pointer(void *raw_va, size_t len, int mode) {
  scoped_lock l(this->lock);
  if (is_kspace) return true;
  off_t start = (off_t)raw_va & PGMASK;
  off_t end = ((off_t)raw_va + len) & PGMASK;

  for (off_t va = start; va <= end; va += PGSIZE) {
    // see if there is a region at the requested offset
    auto r = lookup(va);
    if (!r) {
      // printk(KERN_WARN "validate_pointer(%p) - region not found!\n", raw_va);
      // this->dump();
      return false;
    }

    if ((mode & PROT_READ && !(r->prot & PROT_READ)) ||
        (mode & PROT_WRITE && !(r->prot & PROT_WRITE)) ||
        (mode & PROT_EXEC && !(r->prot & PROT_EXEC))) {
      printk(KERN_WARN "validate_pointer(%p) - protection!\n", raw_va);
      return false;
    }
    // TODO: check mode flags
  }
  return true;
}

bool mm::space::validate_string(const char *str) { return validate_null_terminated(str); }

int mm::space::schedule_mapping(off_t va, off_t pa, int prot) {
  pending_mappings.push({.va = va, .pa = pa, .prot = prot});
  return 0;
}


void mm::space::dump(void) {
  for (struct rb_node *node = rb_first(&regions); node; node = rb_next(node)) {
    auto *r = rb_entry(node, struct mm::area, node);
    printk("%p-%p ", r->va, r->va + r->len);
    printk("%6zupgs", r->mappings.size());
    int mapped = 0;
    for (int i = 0; i < r->mappings.size(); i++)
      if (!r->mappings[i].is_null()) mapped++;
    printk(" %3d%%", (mapped * 100) / r->mappings.size());

    printk("");
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

  for (struct rb_node *node = rb_last(&regions); node; node = rb_prev(node)) {
    auto *r = rb_entry(node, struct mm::area, node);

    auto rva = r->va;
    auto rlim = rva + r->len;

    if (va <= rlim && rva < lim) {
      va = rva - size;
      lim = va + size;
    }
  }

  return va;
}
