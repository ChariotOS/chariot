#include <arch.h>
#include <mm.h>
#include <riscv/arch.h>
#include <riscv/paging.h>

namespace rv {

  class pagetable : public mm::pagetable {
    rv::xsize_t *table;

   public:
    pagetable(rv::xsize_t *table);
    virtual ~pagetable();

    virtual bool switch_to(void) override;

    virtual int add_mapping(off_t va, struct mm::pte &) override;
    virtual int get_mapping(off_t va, struct mm::pte &) override;
    virtual int del_mapping(off_t va) override;
  };
}  // namespace rv

ref<mm::pagetable> mm::pagetable::create() { return nullptr; }




rv::pagetable::pagetable(rv::xsize_t *table) : table(table) {}

rv::pagetable::~pagetable(void) {
  /* TODO free the table! */
  panic("free table %p\n", table);
}


bool rv::pagetable::switch_to(void) {
	/* TODO: */
	return true;
}

int rv::pagetable::add_mapping(off_t va, struct mm::pte &p) {
	/* TODO: */
	return 0;
}


int rv::pagetable::get_mapping(off_t va, struct mm::pte &r) {
	/* TODO: */
	return 0;
}


int rv::pagetable::del_mapping(off_t va) {
	/* TODO: */
	return 0;
}

/* TODO: */
static mm::space *kspace;

mm::space &mm::space::kernel_space(void) {
  if (kspace == NULL) {
		off_t table = read_csr(satp) << 12;
    auto kptable = make_ref<rv::pagetable>((rv::xsize_t*)table);
    kspace = new mm::space(CONFIG_KERNEL_VIRTUAL_BASE, -1, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
