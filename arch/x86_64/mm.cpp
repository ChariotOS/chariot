#include <errno.h>
#include <mm.h>
#include <phys.h>
#include <types.h>

namespace x86 {
class pagetable : public mm::pagetable {
  u64 *pml4;

 public:
  pagetable();
  virtual ~pagetable();

  virtual bool switch_to(void) override;

  virtual int add_mapping(off_t va, struct mm::pte &) override;
  virtual int get_mapping(off_t va, struct mm::pte &) override;
  virtual int del_mapping(off_t va) override;
};
}  // namespace x86

x86::pagetable::pagetable(void) { pml4 = (u64 *)p2v(phys::alloc(1)); }

x86::pagetable::~pagetable(void) {
  // TODO: free up the page table
  phys::free(v2p(pml4), 1);
}

bool x86::pagetable::switch_to(void) { return true; }

ref<mm::pagetable> mm::pagetable::create() {
  return make_ref<x86::pagetable>();
}

int x86::pagetable::add_mapping(off_t va, struct mm::pte &) {
  return -ENOTIMPL;
}
int x86::pagetable::get_mapping(off_t va, struct mm::pte &) {
  return -ENOTIMPL;
}
int x86::pagetable::del_mapping(off_t va) { return -ENOTIMPL; }
