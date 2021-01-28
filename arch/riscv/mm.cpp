#include <arch.h>
#include <mm.h>
#include <phys.h>
#include <riscv/arch.h>
#include <riscv/paging.h>
#include <errno.h>

#define VM_FLAG_BITS 0x3ff
#define VM_PTE_BITS ~(VM_FLAG_BITS)

/* Return a PHYSICAL address to a page newly allocated page table */
static rv::pte_t *alloc_page_table() { return (rv::pte_t *)phys::alloc(1); }

/*
 * Walk the page directory, allocating tables if needed. Returns a pointer to
 * the page table entry at that location (or null, if va is in kernel half)
 */
rv::pte_t *rv::page_walk(rv::pte_t *tbl, off_t va) {
  /* TODO: verify alignment and all that noise */

  /* Validate the address is in the lower half */
  if (va >= CONFIG_KERNEL_VIRTUAL_BASE) return NULL;

  /* Grab a virtual address to the table's page */
  tbl = (rv::xsize_t *)p2v(tbl);

  /* Starting from the highest index in the VA, to zero */
  for (int i = VM_PART_NUM - 1; i >= 0; i--) {
    int index = PX(i, va);
    /* If we are at the end, return the entry :) */
    if (i == 0) return Tp2v(&tbl[index]);
    /* Grab the indirect mapping in the page table */
    auto ent = tbl[index];
    /* If the page isn't valid, map a new page into the entry */
    if ((ent & PT_V) == 0) {
      /* from the priv spec: If all three protection bits are zero,
       * the entry is a pointer to the next level of the page tabl
       */
			auto pa = (rv::xsize_t)phys::alloc(1);
			ent = MAKE_PTE(pa, PT_V);
      tbl[index] = MAKE_PTE(pa, ent);
    }


    tbl = (rv::xsize_t *)p2v((ent & VM_PTE_BITS) << 2);
  }

  return NULL;
}



ref<mm::pagetable> mm::pagetable::create() {
  //
  return nullptr;
}




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

	auto *pte = rv::page_walk(this->table, va);
	if (pte == NULL) return -EINVAL;

	rv::pte_t ent = *pte;
	/* If the page is already mapped,  */
	if ((ent & PT_V) != 0) return -EEXIST;

	int prot = PT_V;
	/* If not a super page, it's user */
	if (p.prot & VPROT_READ) prot |= PT_R;
	if (p.prot & VPROT_WRITE) prot |= PT_W;
	if (p.prot & VPROT_EXEC) prot |= PT_X;
	if (!(p.prot & VPROT_SUPER)) prot |= PT_U;

	/* Write the page table entry */
	*pte = MAKE_PTE(p.ppn << 12, prot);
	/* Flush that bit in the TLB: TODO: tell other cpus? */
	rv::sfence_vma(va);
  return 0;
}


int rv::pagetable::get_mapping(off_t va, struct mm::pte &r) {
	auto *pte = rv::page_walk(this->table, va);
	if (pte == NULL) return -EINVAL;

	rv::pte_t ent = *pte;

	if ((ent & PT_V) == 0) return -ENOENT;

	r.ppn = (ent & VM_PTE_BITS) << 2;
	r.prot = 0;
	if (ent & PT_R) r.prot |= VPROT_READ;
	if (ent & PT_W) r.prot |= VPROT_WRITE;
	if (ent & PT_X) r.prot |= VPROT_EXEC;
	if (!(ent & PT_U)) r.prot |= VPROT_SUPER;
  return 0;
}


int rv::pagetable::del_mapping(off_t va) {
  /* TODO: */
	auto *pte = rv::page_walk(this->table, va);
	if (pte == NULL) return -EINVAL;
	*pte = 0;

	rv::sfence_vma(va);
  return 0;
}

/* TODO: */
static mm::space *kspace;

mm::space &mm::space::kernel_space(void) {
  if (kspace == NULL) {
    off_t table = read_csr(satp) << 12;
    auto kptable = make_ref<rv::pagetable>((rv::xsize_t *)table);
    kspace = new mm::space(CONFIG_KERNEL_VIRTUAL_BASE, -1, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
