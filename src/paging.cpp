#include <asm.h>
#include <mem.h>
#include <paging.h>

#include <phys.h>
#include <printk.h>
#include <types.h>

#define P4_INDEX(addr) ((((u64)addr / 0x1000) >> 27) & 0777)
#define P3_INDEX(addr) ((((u64)addr / 0x1000) >> 18) & 0777)
#define P2_INDEX(addr) ((((u64)addr / 0x1000) >> 9) & 0777)
#define P1_INDEX(addr) ((((u64)addr / 0x1000) >> 0) & 0777)


// #define PAGING_DEBUG

#ifdef PAGING_DEBUG
#define INFO(fmt, args...) printk("[PAGING] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

static inline void flush_tlb_single(u64 addr) {
  __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

u64 *alloc_page_dir(void) {

  auto new_table = (u64 *)phys::alloc();
  INFO("new_table = %p\n", new_table);

  auto va = (u64 *)new_table;

  if (use_kernel_vm) {
    va = (u64 *)p2v(new_table);
  } else {
    paging::map((u64)new_table, (u64)new_table);
  }

  INFO("%p\n", va);
  for (int i = 0; i < 512; i++) va[i] = 0;
  return new_table;
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
    default:
      break;
  }
}

static u64 size_flag(paging::pgsize size) {
  u64 flags = 0;

  switch (size) {
    case paging::pgsize::large:
    case paging::pgsize::huge:
      flags |= PTE_PS;
      break;

    default:
      break;
  }

  return flags;
}


static inline u64 *paging_p2v(u64 *pa) {
  u64 *va = (u64*)(use_kernel_vm ? p2v(pa) : (void *)(pa));
  // printk("paging_p2v(%p) = %p\n", pa, va);
  return va;
}
// convert a page table address to a usable kernel address
#define conv(pta) ((u64 *)(((u64)(pta) & ~0xfff)))

// return the ith page table index for a virtual address
#define pti(va, i) ((((u64)va / 0x1000) >> (9 * i)) & 0777)

u64 *paging::find_mapping(u64 *pml4, u64 va, u64 pa, pgsize size) {
  INFO("============================================\n");
  assert_page_alignment(va, size);
  assert_page_alignment(pa, size);

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

  INFO("depth = %d\n", depth);


  u64 *table = conv(pml4);


  INFO("table[4] = %p\n", table);

  for (int i = 3; i > depth; i--) {
    int ind = pti(va, i);
    if (!(table[ind] & 1)) {
      u64 *new_table = alloc_page_dir();
      INFO("new_table = %p\n", new_table);
      table[ind] = (u64)(new_table) | PTE_P | PTE_W;
    }

    INFO("table(%p)[%d] = %p\n", table, i, table[ind]);

    table = paging_p2v(conv(table[ind]));
  }


  u64 ind = pti(va, depth);
  INFO("index: %llu\n", ind);
  INFO("============================================\n");

  return &table[ind];
}

void paging::map_into(u64 *p4, u64 va, u64 pa, pgsize size, u16 flags) {

  INFO("%p -> %p\n", va, pa);
  u64 *pte = find_mapping(p4, va, pa, size);
  if (*pte & PTE_P) {
    // printk("REMAP!\n");
  }


  // printk("pte: %p\n", pte);
  *pte = (pa & ~0xFFF) | flags | PTE_P | size_flag(size);

  INFO("size_flag = %016x\n", size_flag(size));

  INFO("pte: %p = %p\n", pte, *pte);

  INFO("\n\n");
  flush_tlb_single(va);
}

void paging::map(u64 va, u64 pa, pgsize size, u16 flags) {
  auto p4 = (u64*)read_cr3();
  // printk("mapping %p %p (%d)\n", va, pa, size);
  return paging::map_into(p4, va, pa, size, flags);
}



u64 paging::get_physical(u64 va) {
  return 0;
}
