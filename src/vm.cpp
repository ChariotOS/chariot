#include <vm.h>

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
