#include <errno.h>
#include <phys.h>
#include <vm.h>

vm::phys_page::~phys_page(void) { phys::free((void *)pa); }

ref<vm::phys_page> vm::phys_page::alloc(void) {
  auto p = make_ref<phys_page>();
  p->pa = (u64)phys::alloc();
  return move(p);
}

vm::memory_backing::memory_backing(int npages) {
  // allocate and clear out the pages vector
  pages.ensure_capacity(npages);
  for (int i = 0; i < npages; i++) {
    pages[i] = nullptr;
  }
}

int vm::memory_backing::fault(addr_space &space, region &reg, int page,
                              int flags) {
  if (page < 0 || page > pages.size()) {
    return -ENOENT;
  }

  if (!pages[page]) {
    pages[page] = vm::phys_page::alloc();
  }

  void *va = (void *)((reg.vpage + page) << 12);
  space.schedule_mapping(va, pages[page]->pa);
  return 0;
}

vm::region::region(string name, off_t vpn, size_t len, int prot)
    : name(name), vpage(vpn), len(len), prot(prot) {
  printk("created region '%s' %d:%d prot:%x\n", name.get(), vpn, len, prot);
}

vm::region *vm::addr_space::lookup(off_t vpn) {
  // just dumbly walk over the list of regions and find the right region
  for (auto &r : regions) {
    off_t start = r->vpage;

    if (vpn >= start && vpn < start + r->len) {
      return r.get();
    }
  }

  return nullptr;
}

int vm::addr_space::schedule_mapping(void *va, u64 pa) {
  pending_mappings.push({.va = va, .pa = pa});
  return 0;
}
