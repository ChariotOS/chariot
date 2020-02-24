#include <cpu.h>
#include <errno.h>
#include <fs.h>
#include <paging.h>
#include <phys.h>
#include <util.h>
#include <vm.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

vm::phys_page::~phys_page(void) {
  phys::free((void*)pa);
  pa = 0;
}

ref<vm::phys_page> vm::phys_page::alloc(void) {
  auto p = make_ref<phys_page>();
  p->pa = (u64)phys::alloc();
  return move(p);
}

vm::memory_backing::memory_backing(int npages) {
  // allocate and clear out the pages vector
  // printk("new memory backing: [npages: %d] %p\n", npages, this);
  for (int i = 0; i < npages; i++) {
    pages.push(nullptr);
  }
}

vm::memory_backing::~memory_backing(void) {
  // TODO
}

int vm::memory_backing::fault(addr_space &space, region &reg, int page,
                              int flags) {
  if (page < 0 || page > pages.size()) {
    return -ENOENT;
  }

  if (!pages[page]) {
    pages[page] = vm::phys_page::alloc();
  }

  void *va = (void *)(reg.va + (page << 12));
  space.schedule_mapping(va, pages[page]->pa, reg.prot);
  return 0;
}

vm::file_backing::file_backing(int npages, size_t size, off_t off,
                               struct fs::inode *node)
    : memory_backing(npages),
      off(off),
      size(size),
      fd(node, FDIR_READ | FDIR_WRITE) {
}

vm::file_backing::~file_backing(void) {
  // TODO: handle file backing destruction
}

int vm::file_backing::fault(addr_space &space, region &reg, int page,
                            int flags) {
  if (page < 0 || page >= pages.size()) {
    return -ENOENT;
  }

  if (!pages[page]) {
    pages[page] = vm::phys_page::alloc();

    void *buf = p2v(pages[page]->pa);
    // TODO: handle failures to read

    // offset, in bytes, into the region
    off_t roff = page * PGSIZE;
    fd.seek(off + roff, SEEK_SET);

    size_t to_read = min(PGSIZE, size - roff);
    int nread = fd.read(buf, to_read);

    if (nread == -1) {
      panic("failed to read mapped file!\n");
    }

    if (nread != PGSIZE) {
      // clear out all the other memory that was read
      memset((char *)buf + nread, 0x00, PGSIZE - nread);
    }
  }

  void *va = (void *)(reg.va + (page << 12));
  space.schedule_mapping(va, pages[page]->pa, reg.prot);
  return 0;
}

vm::region::region(string name, off_t va, size_t len, int prot)
    : name(name), va(va), len(len), prot(prot) {
  // printk("created region '%s' %d:%d prot:%x\n", name.get(), va, len, prot);
}

vm::region::~region(void) {
  // release the backing
  backing.clear();
}

vm::region *vm::addr_space::lookup(off_t va) {
  // just dumbly walk over the list of regions and find the right region
  for (auto &r : regions) {
    off_t start = r->va;

    if (va >= start && va < start + r->len) {
      return r;
    }
  }

  return nullptr;
}

// sort regions and return the number of swaps
// TODO: write a better sorter lol
static int sort_regions(vec<vm::region *> &xs) {
  int const len = xs.size();
  if (len == 1) return 0;
  int n = 0;
  for (int i = 1; i < len; i++) {
    for (int j = i; j > 0; j--) {
      auto &t1 = xs.at(j - 1);
      auto &t2 = xs.at(j);
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

static ssize_t convert_protection(off_t vm_prot) {
  ssize_t p = 0;

  p |= PTE_P;

  if (vm_prot & VPROT_READ) p |= PTE_U;
  if (vm_prot & VPROT_WRITE) p |= PTE_W;

  if ((vm_prot & VPROT_EXEC) == 0) p |= PTE_NX;

  p |= PTE_U;

  return p;
}

int vm::addr_space::handle_pagefault(off_t va, int err) {
  auto r = lookup(va);

  if (!r) {
    return -1;
  }

  int fault_res = 0;

  auto page = (va >> 12) - (r->va >> 12);

  // TODO:
  // if (err & PGFLT_USER && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_WRITE && !(r->prot & VPROT_WRITE)) fault_res = -1;
  if (err & FAULT_READ && !(r->prot & VPROT_READ)) fault_res = -1;
  if (err & FAULT_EXEC && !(r->prot & VPROT_EXEC)) fault_res = -1;

  // refer to the backing structure to handle the fault
  if (fault_res == 0) fault_res = r->backing->fault(*this, *r, page, err);

  if (fault_res != 0) {
    /*
    printk("should segfault pid %p for accessing %p illegally\n",
           cpu::proc()->pid, va);
    this->dump();
    */
    return -1;
  }

  auto p4 = (u64 *)p2v(cr3);
  // walk through the scheduled mappings
  for (auto p : pending_mappings) {
    // KWARN("   mapping %p -> %p into %p. was %p\n", p.va, p.pa, cr3, *entry);
    auto flags = convert_protection(p.prot);
    paging::map_into(p4, (u64)p.va, p.pa, paging::pgsize::page, flags | PTE_P);
  }
  pending_mappings.clear();

  arch::flush_tlb();

  return 0;
}

int vm::addr_space::schedule_mapping(void *va, u64 pa, int prot) {
  pending_mappings.push({.va = va, .pa = pa, .prot = prot});
  return 0;
}

int vm::addr_space::set_range(off_t b, off_t l) {
  // virtual addresses are only 48 bits, so we should truncate the addresses
  // to match so we don't go passing ugly pointers around to the process
#define bits(n) ((1LL << n) - 1)
  base = b & ~0xFFF;
  limit = l & ~0xFFF;

  return 0;
}

off_t vm::addr_space::add_mapping(string name, off_t vaddr, size_t size,
                                  ref<vm::memory_backing> mem, int prot) {
  lck.lock();
  auto reg = new vm::region(move(name), vaddr, size, prot);
  reg->backing = mem;

  regions.push(move(reg));
  sort_regions(regions);

  lck.unlock();
  return vaddr;
}

int vm::addr_space::unmap(void *ptr, size_t len) {
  scoped_lock l(lck);

  off_t va = (off_t)ptr;
  if ((va & 0xFFF) != 0) return -1;

  for (int i = 0; i < regions.size(); i++) {
    auto *region = regions[i];
    if (region->va == va && region->len == len) {
      // printk("removing region %d. Named '%s'\n", i, region->name.get());

      regions.remove(i);
      sort_regions(regions);

      auto p4 = (u64 *)p2v(cr3);
      for (off_t v = va; v < va + len; v += 4096) {
        paging::map_into(p4, v, 0, paging::pgsize::page, 0);
      }

      delete region;
      return 0;
    }
  }

  return -1;
}

off_t vm::addr_space::find_region_hole(size_t size) {
  off_t va = limit - size;
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

off_t vm::addr_space::add_mapping(string name, ref<vm::memory_backing> mem,
                                  int prot) {
  // at this point, the address space is sorted because after adding each region
  // it gets sorted Therefore, we need to look at the end for a space that can
  // fit this region's size
  int npages = mem->pages.size();
  int size = npages * PGSIZE;

  lck.lock();

  // TODO: handle OOM
  off_t vaddr = find_region_hole(size);

  // TODO: how to determine failures here?
  if (vaddr == 0) {
    panic("unable to find mapping\n");
  }

  auto reg = new vm::region(move(name), vaddr, size, prot);
  reg->backing = mem;

  regions.push(move(reg));
  sort_regions(regions);

  lck.unlock();

  return vaddr;
}

off_t vm::addr_space::add_mapping(string name, size_t sz, int prot) {
  sz = round_up(sz, PGSIZE);
  auto mem = make_ref<vm::memory_backing>(sz / PGSIZE);
  return add_mapping(name, mem, prot);
}

off_t vm::addr_space::map_file(string name, struct fs::inode *file, off_t vaddr,
                               off_t off, size_t size, int prot) {
  auto sz = round_up(size, 4096);
  return add_mapping(name, vaddr & ~0xFFF, sz,
                     make_ref<vm::file_backing>(sz >> 12, size, off, file),
                     prot);
}

#define PGMASK (~(PGSIZE - 1))
bool vm::addr_space::validate_pointer(void *raw_va, size_t len, int mode) {
  if (kernel_vm) return true;
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

bool vm::addr_space::validate_string(const char *str) {
  // TODO: this is really unsafe
  if (validate_pointer((void *)str, 1, VALIDATE_READ)) {
    int len = 0;
    for (len = 0; str[len] != '\0'; len++)
      ;
    return validate_pointer((void *)str, len, VALIDATE_READ);
  }

  return false;
  // TODO: this is really slow.
  auto va = (off_t)str;

  bool page_validated = false;
  ssize_t validated_page = 0;
  while (1) {
    if (validated_page == (va & PGMASK) && page_validated) {
      char *c = (char *)va;
      if (c[0] == '\0') {
        return true;
      }
      va++;
    } else {
      // TODO: validate page
    }
  }

  return true;
}

vm::addr_space::addr_space(void) {
  // process base/limit
  set_range(0, KERNEL_VIRTUAL_BASE);

  // allocate a page for the page directory
  cr3 = phys::alloc(1);
}

vm::addr_space::~addr_space(void) {
  // this->dump();
  for (auto r : regions) delete r;

  // free the layers of the page table and cr3
  paging::free_table(cr3);
}

string vm::addr_space::format(void) {
  string s;

  for (auto &r : regions) {
    s += string::format("%p-%p ", r->va, r->va + r->len);
    s += string::format("%c", r->prot & VPROT_READ ? 'r' : '-');
    s += string::format("%c", r->prot & VPROT_WRITE ? 'w' : '-');
    s += string::format("%c", r->prot & VPROT_EXEC ? 'x' : '-');
    s += string::format(" %s", r->name.get());
    s += string::format("\n");
  }
  return s;
}

void vm::addr_space::dump(void) {
  if (lck.is_locked()) {
    printk("    VA: locked.\n");
    return;
  }

  printk("%s", format().get());
}
