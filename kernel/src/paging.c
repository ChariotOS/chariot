#include <asm.h>
#include <paging.h>
#include <types.h>


#define P4_INDEX(addr) ((((u64)addr / 0x1000) >> 27) & 0777)
#define P3_INDEX(addr) ((((u64)addr / 0x1000) >> 18) & 0777)
#define P2_INDEX(addr) ((((u64)addr / 0x1000) >> 9) & 0777)
#define P1_INDEX(addr) ((((u64)addr / 0x1000) >> 0) & 0777)

// Converts a virtual address into its physical address
//    This is some scary code lol
void *get_physaddr(void *va) {
  // cr3 contains a pointer to p4
  u64 *p4_table = (u64*)read_cr3();
  // read the p4 table to get the p3 table
  u64 p3_table = p4_table[P4_INDEX(va)];
  if ((p3_table & 1) == 0) return PGERR; // check for active
  u64 p2_table = ((u64*)(p3_table & ~0xfff))[P3_INDEX(va)];
  if ((p2_table & 1) == 0) return PGERR; // check for active
  u64 p1_table = ((u64*)(p2_table & ~0xfff))[P2_INDEX(va)];
  if ((p1_table & 1) == 0) return PGERR; // check for active
  u64 pa = ((u64*)(p1_table & ~0xfff))[P1_INDEX(va)];
  if ((pa & 1) == 0) return PGERR; // check for active
  return (void*)((pa & ~0xfff) | ((u64)va & 0xfff));
}
