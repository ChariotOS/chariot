#include <asm.h>
#include <mem.h>
#include <paging.h>

#include <printk.h>
#include <types.h>

#define P4_INDEX(addr) ((((u64)addr / 0x1000) >> 27) & 0777)
#define P3_INDEX(addr) ((((u64)addr / 0x1000) >> 18) & 0777)
#define P2_INDEX(addr) ((((u64)addr / 0x1000) >> 9) & 0777)
#define P1_INDEX(addr) ((((u64)addr / 0x1000) >> 0) & 0777)



static inline void flush_tlb_single(u64 addr)
{
   __asm__ volatile("invlpg (%0)" ::"r" (addr) : "memory");
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


void *v2p(void *va) {
  return do_pagewalk(va).pa;
}

u64 *alloc_page_dir(void) {
  auto new_table = (u64*)alloc_id_page();
  for (int i = 0; i < 512; i++) new_table[i] = 0;
  return new_table;
}

void map_page_into(u64 *p4, void *va, void *pa) {

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
void map_page(void *va, void *pa) {
  u64 *p4 = (u64 *)read_cr3();
  // printk("mapping from %p to %p (CR3 = %p)\n", va, pa, p4);
  map_page_into(p4, va, pa);
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

void print_page_dir(u64 *p4) { do_print_page_dir(p4, 4); }
