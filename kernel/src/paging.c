#include <asm.h>
#include <mem.h>
#include <paging.h>

#include <printk.h>
#include <types.h>

#define P4_INDEX(addr) ((((u64)addr / 0x1000) >> 27) & 0777)
#define P3_INDEX(addr) ((((u64)addr / 0x1000) >> 18) & 0777)
#define P2_INDEX(addr) ((((u64)addr / 0x1000) >> 9) & 0777)
#define P1_INDEX(addr) ((((u64)addr / 0x1000) >> 0) & 0777)

// Converts a virtual address into its physical address
//    This is some scary code lol
void *get_physaddr(void *va) {
  // cr3 contains a pointer to p4
  u64 *p4_table = (u64 *)read_cr3();
  // read the p4 table to get the p3 table
  u64 p3_table = p4_table[P4_INDEX(va)];
  if ((p3_table & 1) == 0) return PGERR;  // check for active
  u64 p2_table = ((u64 *)(p3_table & ~0xfff))[P3_INDEX(va)];
  if ((p2_table & 1) == 0) return PGERR;  // check for active
  u64 p1_table = ((u64 *)(p2_table & ~0xfff))[P2_INDEX(va)];
  if ((p1_table & 1) == 0) return PGERR;  // check for active
  u64 pa = ((u64 *)(p1_table & ~0xfff))[P1_INDEX(va)];
  if ((pa & 1) == 0) return PGERR;  // check for active
  return (void *)((pa & ~0xfff) | ((u64)va & 0xfff));
}

u64 *alloc_page_dir(void) {
  u64 *new_table = alloc_page();
  for (int i = 0; i < 512; i++) new_table[i] = 0;
  return new_table;
}

void map_page_into(u64 *p4, void *va, void *pa) {
  u64 *table = (u64 *)((u64)p4 & ~0xfff);

  for (int i = 3; i > 0; i--) {
    // the index within the current table
    int ind = ((((u64)va / 0x1000) >> (9 * i)) & 0777);
    bool new = false;

    if (i != 0 && (table[ind] & 1) == 0) {
      u64 *new_table = alloc_page_dir();
      table[ind] = (u64)new_table | 0x3;
      new = true;
    }

    table = (u64 *)(table[ind] & ~0xfff);
  }

  table[((u64)va / 0x1000) & 0777] = (u64)pa | 0x3;
  // printk("\n");
}

// map a page in the current p4_table env
void map_page(void *va, void *pa) {
  u64 *p4 = (u64 *)read_cr3();
  map_page_into(p4, va, pa);
}
