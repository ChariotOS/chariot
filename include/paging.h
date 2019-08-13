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



#define PAGE_SIZE 0x1000
#define LARGE_PAGE_SIZE 0x200000
#define HUGE_PAGE_SIZE 0x40000000

namespace paging {

  enum class pgsize : u8 {
    page = 0,
    large = 1,
    huge = 3,
    unknown = 4
  };

  u64 *find_mapping(u64 *p4, u64 va, u64 pa, pgsize size);


  void map_into(u64 *p4, u64 va, u64 pa, pgsize size, u16 flags);
  void map(u64 va, u64 pa, pgsize size = pgsize::page, u16 flags = PTE_W | PTE_P);


  u64 get_physical(u64 va);
};

#endif
