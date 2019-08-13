#include <asm.h>
#include <mem.h>
#include <paging.h>

#include <printk.h>
#include <types.h>

#define P4_INDEX(addr) ((((u64)addr / 0x1000) >> 27) & 0777)
#define P3_INDEX(addr) ((((u64)addr / 0x1000) >> 18) & 0777)
#define P2_INDEX(addr) ((((u64)addr / 0x1000) >> 9) & 0777)
#define P1_INDEX(addr) ((((u64)addr / 0x1000) >> 0) & 0777)

static inline void flush_tlb_single(u64 addr) {
  __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

// Converts a virtual address into its physical address
//    This is some scary code lol
struct page_mapping do_pagewalk(void *va) {
#define INVALID_PAGE_MAPPING ((struct page_mapping){.pa = 0, .valid = false})
  // cr3 contains a pointer to p4
  u64 *p4_table = (u64 *)read_cr3();
  // read the p4 table to get the p3 table
  u64 p3_table = p4_table[P4_INDEX(va)];
  if ((p3_table & 1) == 0) return INVALID_PAGE_MAPPING;  // check for active
  u64 p2_table = ((u64 *)(p3_table & ~0xfff))[P3_INDEX(va)];
  if ((p2_table & 1) == 0) return INVALID_PAGE_MAPPING;  // check for active
  u64 p1_table = ((u64 *)(p2_table & ~0xfff))[P2_INDEX(va)];
  if ((p1_table & 1) == 0) return INVALID_PAGE_MAPPING;  // check for active
  u64 pa = ((u64 *)(p1_table & ~0xfff))[P1_INDEX(va)];
  if ((pa & 1) == 0) return INVALID_PAGE_MAPPING;  // check for active

  struct page_mapping pm;
  pm.valid = true;
  pm.pa = (void *)((pa & ~0xfff) | ((u64)va & 0xfff));

  return pm;

#undef INVALID_PAGE_MAPPING
}

u64 *alloc_page_dir(void) {
  printk("alloc page dir\n");
  auto new_table = (u64 *)alloc_id_page();
  for (int i = 0; i < 512; i++) new_table[i] = 0;
  return new_table;
}

void map_page_into(u64 *p4, void *va, void *pa, u16 flags) {
  u64 *table = (u64 *)((u64)p4 & ~0xfff);
  for (int i = 3; i > 0; i--) {
    // the index within the current table
    // this is some spooky math, but look at the PN_INDEX macros at the
    // top of this file for more information
    int ind = ((((u64)va / 0x1000) >> (9 * i)) & 0777);
    if (i != 0 && (table[ind] & 1) == 0) {
      u64 *new_table = alloc_page_dir();
      table[ind] = (u64)new_table | 0x3;
    }
    table = (u64 *)(table[ind] & ~0xfff);
  }
  u64 ind = ((u64)va / 0x1000) & 0777;

  if (table[ind] & 1) {
    // printk("remapping\n");
  }

  table[ind] = (u64)pa | 0x3;

  flush_tlb_single((u64)va);
}

// map a page in the current p4_table env
void map_page(void *va, void *pa, u16 flags) {
  u64 *p4 = (u64 *)read_cr3();
  // printk("mapping from %p to %p (CR3 = %p)\n", va, pa, p4);
  map_page_into(p4, va, pa, flags);
}

static void do_print_page_dir(u64 *p, int lvl) {
  if (lvl == 0) {
    for (int i = 0; i < 512; i++) {
      u64 pa = p[i];
      if ((pa & 1) == 0) continue;
      printk("\t\t\t\t%p\n", pa & ~0xfff);
    }
    return;
  }

  for (int i = 0; i < 512; i++) {
    u64 pa = p[i];

    if ((pa & 1) == 0) continue;
    for (int j = lvl - 4; j > 0; j--) printk("\t");
    printk("%p\n", pa);
    // do_print_page_dir((u64*)pa, lvl - 1);
  }
}

static void assert_page_size_alignment(u64 addr, u64 psize, const char *str) {
  if ((addr & (psize - 1)) != 0) {
    panic("[PAGING] address %p is not aligned correctly for %s mapping\n", addr,
          str);
  }
}
static void assert_page_alignment(u64 addr, paging::pgsize size) {
  switch (size) {
    case paging::pgsize::page:
      assert_page_size_alignment(addr, PAGE_SIZE, "4kb");
      break;

    case paging::pgsize::large:
      assert_page_size_alignment(addr, LARGE_PAGE_SIZE, "2mb");
      break;

    case paging::pgsize::huge:
      assert_page_size_alignment(addr, HUGE_PAGE_SIZE, "1gb");
      break;
  }
}

static u64 size_flag(paging::pgsize size) {
  u64 flags = 0;

  switch (size) {
    case paging::pgsize::large:
      flags |= PTE_PS;
      break;

    case paging::pgsize::huge:
      flags |= PTE_PS;
      break;

    default:
      break;
  }

  return flags;
}


#define paging_p2v(pa) (use_kernel_vm ? p2v(pa) : (void*)(pa))
// convert a page table address to a usable kernel address
#define conv(pta) (u64 *)((u64)(paging_p2v(pta)) & ~0xfff)

// return the ith page table index for a virtual address
#define pti(va, i) ((((u64)va / 0x1000) >> (9 * i)) & 0777)

u64 *paging::find_mapping(u64 *pml4, u64 va, u64 pa, pgsize size, u16 flags) {
  assert_page_alignment(va, size);
  assert_page_alignment(pa, size);

  pml4 = conv(pml4);

  int depth;

  switch (size) {
    case paging::pgsize::large:
      depth = 1;
      break;

    case paging::pgsize::huge:
      depth = 2;
      break;

    default:
      depth = 0;
      break;
  }

  u64 *table = conv(pml4);
  for (int i = 3; i > depth; i--) {
    int ind = pti(va, i);
    if (i != 0 && (table[ind] & 1) == 0) {
      u64 *new_table = alloc_page_dir();
      table[ind] = (u64)new_table | 0x3;
    }

    table = conv(table[ind]);
  }
  u64 ind = pti(va, depth);

  // if (table[ind] & 1 && (flags & PTE_P)) panic("remap!\n");

  return &table[ind];
  // table[ind] = (u64)pa | flags;
  // flush_tlb_single((u64)va);
}

void paging::map_into(u64 *p4, u64 va, u64 pa, pgsize size, u16 flags) {
  u64 *pte = find_mapping(p4, va, pa, size, flags | PTE_P);
  *pte = pa | flags | PTE_P | size_flag(size);
  flush_tlb_single(va);
}

void paging::map(u64 va, u64 pa, pgsize size, u16 flags) {
  return paging::map_into((u64 *)read_cr3(), va, pa, size, flags);
}

void print_page_dir(u64 *p4) { do_print_page_dir(p4, 4); }
