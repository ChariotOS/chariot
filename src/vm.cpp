#include <errno.h>
#include <phys.h>
#include <vm.h>
#include <paging.h>

vm::phys_page::~phys_page(void) { phys::free((void *)pa); }

ref<vm::phys_page> vm::phys_page::alloc(void) {
  auto p = make_ref<phys_page>();
  p->pa = (u64)phys::alloc();
  return move(p);
}

vm::memory_backing::memory_backing(int npages) {
  // allocate and clear out the pages vector

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
  space.schedule_mapping(va, pages[page]->pa);
  return 0;
}

vm::region::region(string name, off_t va, size_t len, int prot)
    : name(name), va(va), len(len), prot(prot) {
  printk("created region '%s' %d:%d prot:%x\n", name.get(), va, len, prot);
}

vm::region *vm::addr_space::lookup(off_t va) {
  // just dumbly walk over the list of regions and find the right region
  for (auto &r : regions) {
    off_t start = r->va;

    if (va >= start && va < start + r->len) {
      return r.get();
    }
  }

  return nullptr;
}


// sort regions and return the number of swaps
// TODO: write a better sorter lol
static int sort_regions(vec<unique_ptr<vm::region>> &xs) {
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

int vm::addr_space::handle_pagefault(off_t va, int flags) {
  auto r = lookup(va);


  if (!r) return -1;

  auto page = (va >> 12) -  (r->va >> 12);

  r->backing->fault(*this, *r, page, flags);

  // walk through the scheduled mappings
  for (auto p : pending_mappings) {
    KINFO("mapping %p -> %p into %p\n", p.va, p.pa, cr3);
    paging::map_into((u64*)p2v(cr3), (u64)p.va, p.pa, paging::pgsize::page, PTE_W | PTE_W | PTE_U);
  }

  pending_mappings.clear();
  return -1;
}

int vm::addr_space::schedule_mapping(void *va, u64 pa) {
  pending_mappings.push({.va = va, .pa = pa});
  return 0;
}

int vm::addr_space::set_range(off_t b, off_t l) {
  if (b < l) return -1;

  base = b;
  limit = l;

  return 0;
}


off_t vm::addr_space::add_mapping(string name, off_t vaddr, size_t size, ref<vm::memory_backing> mem, int prot) {


  lck.lock();
  auto reg = make_unique<vm::region>(move(name), vaddr, size, prot);
  reg->backing = mem;

  regions.push(move(reg));
  sort_regions(regions);

  lck.unlock();
  return vaddr;
}


off_t vm::addr_space::add_mapping(string name, ref<vm::memory_backing> mem, int prot) {
  // at this point, the address space is sorted because after adding each region it gets sorted
  // Therefore, we need to look at the end for a space that can fit this region's size
  off_t vaddr = 0;
  int npages = mem->pages.size();
  int size = npages * PGSIZE;


  lck.lock();

  // walk backwards from the top of the address space and see if there is a spot
  for (int i = regions.size() - 1; i >= 0; i--) {

  }

  // TODO: how to determine failures here?
  if (vaddr == 0) {
    panic("unable to find mapping\n");
  }

  auto reg = make_unique<vm::region>(move(name), vaddr, size, prot);
  reg->backing = mem;

  regions.push(move(reg));
  sort_regions(regions);

  lck.unlock();

  return vaddr;
}



vm::addr_space::addr_space(void) : lck("addr_space_lock") {
  // process base/limit
  set_range(0, KERNEL_VIRTUAL_BASE);

  KINFO("new addr_space\n");


  // allocate a page for the page directory
  cr3 = phys::alloc(1);

}

vm::addr_space::~addr_space(void) { KINFO("destruct addr_space\n"); }
