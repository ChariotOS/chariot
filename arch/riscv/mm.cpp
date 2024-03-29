#include <arch.h>
#include <errno.h>
#include <mm.h>
#include <phys.h>
#include <riscv/arch.h>
#include <riscv/sbi.h>
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
    // printf("[pwalk] %d: %d\n", i, index);
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



ck::ref<mm::PageTable> mm::PageTable::create() {
  auto p = (rv::xsize_t *)p2v(phys::alloc(1));
  return ck::make_ref<rv::PageTable>(p);
}


rv::PageTable::PageTable(rv::xsize_t *table) : table(table) {
  /* Copy the top half (global, one time map) from the current
   * page table. All tables have this mapping, so we can do this safely
   */
  auto kptable = (rv::xsize_t *)p2v(read_csr(satp) << 12);
  auto pptable = (rv::xsize_t *)p2v(table);

  int entries = 4096 / sizeof(rv::xsize_t);
  int half = entries / 2;

  for (int i = half; i < entries; i++)
    pptable[i] = kptable[i];
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

rv::PageTable::~PageTable(void) {
  auto pptable = (rv::xsize_t *)p2v(table);

  int entries = 4096 / sizeof(rv::xsize_t);
  int half = entries / 2;

  /* Go through the bottom half of the top page table */
  for (int i = 0; i < half; i++) {
    auto pte = pptable[i];
    if (pte == 0) continue;
    if ((pte & PT_V) && (pte & (PT_R | PT_W | PT_X)) == 0) {
      auto pa = PTE2PA(pte);
      free_pd((rv::xsize_t *)p2v(pa), 1);
    }
  }

  phys::free(v2p(table));
}



bool rv::PageTable::switch_to(void) {
  // Atoimically set this HART's bit in the pagetables hart mask so we know
  // what cores to flush when we commiting mappings
  __atomic_or_fetch(&hart_mask, (1LU << cpu::current().id), __ATOMIC_ACQ_REL);

  //
  write_csr(satp, MAKE_SATP(v2p(table)));
  //

  return true;
}


static __inline void sbi_remote_sfence_vma(const unsigned long *hart_mask, unsigned long start, unsigned long size) {
  (void)sbi_call(SBI_REMOTE_SFENCE_VMA, (uint64_t)hart_mask, start, size);
}

void rv::PageTable::commit_mappings(ck::vec<mm::PendingMapping> &mappings) {
  for (auto &mapping : mappings) {
    auto &p = mapping.pte;
    auto va = mapping.va;

    volatile auto *pte = rv::page_walk(this->table, va);
    if (pte == NULL) {
      panic("invalid mapping commit. pte = NULL\n");
    }

    rv::pte_t ent = *pte;

    int prot = PT_V;

    if (mapping.cmd == mm::PendingMapping::Command::Map) {
      /*
       * due to interesting decisions by the spec, and a hard to track down
       * bug on the HiFive unleashed, there are two schemes by which the processor
       * is allowed to manage A and D bits:
       *
       * - When a virtual page is accessed and the A bit is clear, or is written and the D bit is clear,
       *   the implementation sets the corresponding bit in the PTE. The PTE update must be atomic
       *   with respect to other accesses to the PTE, and must atomically check that the PTE is valid
       *   and grants sufficient permissions. The PTE update must be exact (i.e., not speculative), and
       *   observed in program order by the local hart. The ordering on loads and stores provided by
       *   FENCE instructions and the acquire/release bits on atomic instructions also orders the PTE
       *   updates associated with those loads and stores as observed by remote harts
       *
       * - When a virtual page is accessed and the A bit is clear, or is written and the D bit is clear, a
       *   page-fault exception is raised
       *
       * The second method here is arguably easier when developing hardware, but also acts as a "lower bound"
       * on software implementation. We therefore set PT_A and PT_D here, as it doesn't result in any kind of
       * negative side-effects in the case of the first method. This kernel, at the very least, doesn't use
       * these bits, and depends on write-based page faults to determine how to manage copy on write.
       *
       * source: https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf
       *         page 61
       *
       * that said, we do this:
       */
      prot |= PT_A | PT_D;

      if (p.prot & VPROT_READ) prot |= PT_R;
      if (p.prot & VPROT_WRITE) prot |= PT_W;
      if (p.prot & VPROT_EXEC) prot |= PT_X;
      /* If not a super page, it's user */
      if (!(p.prot & VPROT_SUPER)) prot |= PT_U;

      auto old_pa = PTE2PA(ent);

      /* Write the page table entry */
      *pte = MAKE_PTE(p.ppn << 12, prot);
      __sync_synchronize();
    } else if (mapping.cmd == mm::PendingMapping::Command::Delete) {
      *pte = 0;
    }
  }


  // now invalidate the other cores' tlbs
  for (int i = 0; i < mappings.size(); i++) {
    off_t va = mappings[i].va;
    size_t pages = 1;

    for (int n = i + 1; n < mappings.size(); n++) {
      auto &nva = mappings[n].va;
      if (nva == va + (pages * 4096)) {
				pages++;
      } else {
        break;
      }
    }

		remote_invalidations++;

    sbi_remote_sfence_vma(&hart_mask, va, pages * 4096);
  }
}

int rv::PageTable::get_mapping(off_t va, struct mm::pte &r) {
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


/* TODO: */
static mm::AddressSpace *kspace;

mm::AddressSpace &mm::AddressSpace::kernel_space(void) {
  if (kspace == NULL) {
    off_t table = read_csr(satp) << 12;
    auto kptable = ck::make_ref<rv::PageTable>((rv::xsize_t *)p2v(table));
    kspace = new mm::AddressSpace(CONFIG_KERNEL_VIRTUAL_BASE, -1, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
