#include <arch.h>
#include <errno.h>
#include <mm.h>
#include <phys.h>
#include <riscv/arch.h>
#include <riscv/paging.h>
#include <util.h>

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
    // printk("[pwalk] %d: %d\n", i, index);
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
  auto p = (rv::xsize_t *)p2v(phys::alloc(1));
  return make_ref<rv::pagetable>(p);
}


rv::pagetable::pagetable(rv::xsize_t *table) : table(table) {
  /* Copy the top half (global, one time map) from the current
   * page table. All tables have this mapping, so we can do this safely
   */
  auto kptable = (rv::xsize_t *)p2v(read_csr(satp) << 12);
  auto pptable = (rv::xsize_t *)p2v(table);

  int entries = 4096 / sizeof(rv::xsize_t);
  int half = entries / 2;

  for (int i = half; i < entries; i++) pptable[i] = kptable[i];
}

static void free_pd(rv::xsize_t *pd, int depth) {
    for (int i = 0; i < 4096 / sizeof(rv::xsize_t); i++) {
      auto pte = pd[i];
      if ((pte & PT_V) && (pte & (PT_R | PT_W | PT_X)) == 0) {
        auto pa = PTE2PA(pte);
        free_pd((rv::xsize_t *)p2v(pa), depth + 1);
      }
    }
    phys::free(v2p(pd), 1);
}

rv::pagetable::~pagetable(void) {
  auto pptable = (rv::xsize_t *)p2v(table);

	// printk(KERN_INFO "nfree: %d\n", phys::nfree());
  int entries = 4096 / sizeof(rv::xsize_t);
  int half = entries / 2;

  /* Go through the bottom half of the top page table */
  for (int i = 0; i < half; i++) {
    auto pte = pptable[i];
    if (pte == 0) continue;
    if ((pte & PT_V) && (pte & (PT_R | PT_W | PT_X)) == 0) {
      // printk(KERN_INFO "%d: %p\n", i, pte);
      auto pa = PTE2PA(pte);
      free_pd((rv::xsize_t *)p2v(pa), 1);
    }
  }

	phys::free(v2p(table));
}



bool rv::pagetable::switch_to(void) {
  // printk_nolock("switch to %p\n", table);
  write_csr(satp, MAKE_SATP(v2p(table)));
  return true;
}

int rv::pagetable::add_mapping(off_t va, struct mm::pte &p) {
  /* TODO: */

  volatile auto *pte = rv::page_walk(this->table, va);
  if (pte == NULL) return -EINVAL;

  rv::pte_t ent = *pte;

  int prot = PT_V;
  /* If not a super page, it's user */
  if (p.prot & VPROT_READ) prot |= PT_R;
  if (p.prot & VPROT_WRITE) prot |= PT_W;
  if (p.prot & VPROT_EXEC) prot |= PT_X;
  if (!(p.prot & VPROT_SUPER)) prot |= PT_U;

  auto old_pa = PTE2PA(ent);

#if 0
  printk(KERN_INFO "[pid=%d] add mapping 0x%llx to 0x%llx ", curproc->pid, va, p.ppn << 12);
  if (old_pa != 0) {
    printk("old pa: 0x%llx  ", old_pa);
  }
  if (ent & PT_R) printk("R");
  if (ent & PT_W) printk("W");
  if (ent & PT_X) printk("X");
  if (ent & PT_U) printk("U");
  if (ent & PT_G) printk("G");
  printk(" -> ");
  if (prot & PT_R) printk("R");
  if (prot & PT_W) printk("W");
  if (prot & PT_X) printk("X");
  if (prot & PT_U) printk("U");
  if (prot & PT_G) printk("G");
  printk("\n");
#endif

  /* Write the page table entry */
  *pte = MAKE_PTE(p.ppn << 12, prot);

  // printk(KERN_INFO "ent=%p, new=%p\n", ent, *pte);

  asm volatile("" ::: "memory");

  if (*pte != ent) {
    /* Flush that bit in the TLB: TODO: tell other cpus? */
    rv::sfence_vma(va);
  }
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
    auto kptable = make_ref<rv::pagetable>((rv::xsize_t *)p2v(table));
    kspace = new mm::space(CONFIG_KERNEL_VIRTUAL_BASE, -1, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
