#ifndef __PAGING__
#define __PAGING__

#include <mm.h>
#include <types.h>

#define PGERR ((void *)-1)


#define PTE_P 0x001    // Present
#define PTE_W 0x002    // Writeable
#define PTE_U 0x004    // User
#define PTE_PWT 0x008  // Write-Through
#define PTE_PCD 0x010  // Cache-Disable
#define PTE_A 0x020    // Accessed
#define PTE_D 0x040    // Dirty
#define PTE_PS 0x080   // Page Size
#define PTE_G 0x100    // Global Mapping (dont flush from tlb)

#define PTE_NX (1LLU << 63)  // no execute



#define PAGE_SIZE 0x1000
#define LARGE_PAGE_SIZE 0x200000
#define HUGE_PAGE_SIZE 0x40000000


namespace x86 {
  class PageTable : public mm::PageTable {
    u64 *pml4;
		uint64_t ptid; // the id of this page table
		bool in_transaction = false;
		const char *tx_reason = NULL;

    spinlock lock;

    struct pending_mapping {
      off_t va;
			mm::pte pte;
    };
		ck::vec<pending_mapping> pending_mappings;

   public:
    PageTable(u64 *pml4);
    virtual ~PageTable();

    bool switch_to(void) override;

    int add_mapping(off_t va, struct mm::pte &) override;
    int get_mapping(off_t va, struct mm::pte &) override;
    int del_mapping(off_t va) override;

		void transaction_begin(const char *reason = "unknown") override;
		void transaction_commit() override;
  };


  enum class pgsize : u8 { page = 0, large = 1, huge = 3, unknown = 4 };

  u64 *find_mapping(u64 *p4, u64 va, pgsize size);
  void dump_page_table(u64 *p4);
  void map_into(u64 *p4, u64 va, u64 pa, pgsize size, u64 flags);
  void map(u64 va, u64 pa, pgsize size = pgsize::page, u64 flags = PTE_W | PTE_P);

  void free_table(void *);
  u64 get_physical(u64 va);
}  // namespace x86

#endif
