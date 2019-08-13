#ifndef __PAGING__
#define __PAGING__

#include <types.h>

#define PGERR ((void *)-1)


#define PTE_P           0x001   // Present
#define PTE_W           0x002   // Writeable
#define PTE_U           0x004   // User
#define PTE_PWT         0x008   // Write-Through
#define PTE_PCD         0x010   // Cache-Disable
#define PTE_A           0x020   // Accessed
#define PTE_D           0x040   // Dirty
#define PTE_PS          0x080   // Page Size
#define PTE_G           0x100   // Global Mapping (dont flush from tlb)

struct page_mapping {
  void *pa;
  bool valid;
};
// Converts a virtual address into its physical address
//    This is some scary code lol
struct page_mapping do_pagewalk(void *va);

void map_page_into(u64 *p4, void *va, void *pa, u16 flags = 0);
void map_page(void *va, void *pa, u16 flags = 0);


#define PAGE_SIZE 0x1000
#define LARGE_PAGE_SIZE 0x200000
#define HUGE_PAGE_SIZE 0x40000000

namespace paging {


  enum class pgsize : u8 {
    page = 0,
    large = 1,
    huge = 3,
  };



  u64 *find_mapping(u64 *p4, u64 va, u64 pa, pgsize size, u16 flags);


  void map_into(u64 *p4, u64 va, u64 pa, pgsize size, u16 flags);
  void map(u64 va, u64 pa, pgsize size = pgsize::page, u16 flags = PTE_W | PTE_P);
};

#endif
