#include <asm.h>
#include <mem.h>
#include <x86/mm.h>
#include <phys.h>
#include <printf.h>
#include <types.h>
#include <crypto.h>

// #define PAGING_DEBUG

static inline void flush_tlb_single(u64 addr) { __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory"); }

u64 *alloc_page_dir(void) { return (u64 *)phys::alloc(); }

static void assert_page_size_alignment(u64 addr, u64 psize, const char *str) {
  if ((addr & (psize - 1)) != 0) {
    panic("[PAGING] address %p is not aligned correctly for %s mapping\n", addr, str);
  }
}
static void assert_page_alignment(u64 addr, x86::pgsize size) {
  switch (size) {
    case x86::pgsize::page:
      assert_page_size_alignment(addr, PAGE_SIZE, "4kb");
      break;

    case x86::pgsize::large:
      assert_page_size_alignment(addr, LARGE_PAGE_SIZE, "2mb");
      break;

    case x86::pgsize::huge:
      assert_page_size_alignment(addr, HUGE_PAGE_SIZE, "1gb");
      break;
    default:
      break;
  }
}

static u64 size_flag(x86::pgsize size) {
  u64 flags = 0;

  switch (size) {
    case x86::pgsize::large:
    case x86::pgsize::huge:
      flags |= PTE_PS;
      break;

    default:
      break;
  }

  return flags;
}

static inline u64 *paging_p2v(u64 *pa) {
  u64 *va = (u64 *)p2v(pa);
  // printf("paging_p2v(%p) = %p\n", pa, va);
  return va;
}
// convert a page table address to a usable kernel address
#define conv(pta) ((u64 *)(((u64)(pta) & ~0xfff)))

// return the ith page table index for a virtual address
#define pti(va, i) ((((u64)va >> 12) >> (9 * i)) & 0777)

u64 *x86::find_mapping(u64 *pml4, u64 va, pgsize size) {
  assert_page_alignment(va, size);
  int depth;

  switch (size) {
    case x86::pgsize::large:
      depth = 1;
      break;

    case x86::pgsize::huge:
      depth = 2;
      break;

    default:
      depth = 0;
      break;
  }

  u64 *table = conv(pml4);

  for (int i = 3; i > depth; i--) {
    int ind = pti(va, i);
    if (!(table[ind] & 1)) {
      u64 *new_table = alloc_page_dir();

      int pflags = PTE_P | PTE_W;
      if (va < CONFIG_KERNEL_VIRTUAL_BASE) {
        pflags |= PTE_U;
      }
      table[ind] = (u64)(new_table) | pflags;
    }

    table = paging_p2v(conv(table[ind]));
  }

  u64 ind = pti(va, depth);

  return &table[ind];
}

void x86::dump_page_table(u64 *p4) {
  for_range(i, 0, 512) {
    u64 entry = p4[i];
    if (entry & PTE_P) {
      printf("  %3d: %p\n", i, p4[i]);
    }
  }
}

#define NOISE_BITS 8
#define BITS(N) ((1 << (N)) - 1)




void x86::map_into(u64 *p4, u64 va, u64 pa, pgsize size, u64 flags) {
  static uint64_t i = 0;
  u64 *pte = find_mapping(p4, va, size);

  uint64_t noise = (i++) & BITS(NOISE_BITS);

  *pte = (pa & ~0xFFF) | flags | size_flag(size) | (noise << (64 - 1 - NOISE_BITS));
	/*
	for_range(i, 63, 0) {
		printf("%d", ((*pte) >> i) & 0b1);
	}
  printf(" %p -> %llp %*b\n", va, *pte, NOISE_BITS, noise);
	*/

  flush_tlb_single(va);
}

void x86::map(u64 va, u64 pa, pgsize size, u64 flags) {
  auto p4 = (u64 *)p2v(read_cr3());
  return x86::map_into(p4, va, pa, size, flags);
}

u64 x86::get_physical(u64 va) { return 0; }

static void free_p2(off_t *p2_p) {
  off_t *p2 = (off_t *)p2v(p2_p);
  for (int i = 0; i < 512; i++) {
    if (p2[i]) {
      off_t e = p2[i];
      if ((e & PTE_P) == 0) continue;
      if (e & PTE_PS) {
        phys::free((void *)(e & ~0xFFF));
        continue;
      }
      phys::free((off_t *)(e & ~0xFFF));
    }
  }
  phys::free(p2_p);
}

static void free_p3(off_t *p3_p) {
  off_t *p3 = (off_t *)p2v(p3_p);
  for (int i = 0; i < 512; i++) {
    if (p3[i]) {
      off_t e = p3[i];

      if ((e & PTE_P) == 0) continue;
      if (e & PTE_PS) {
        phys::free((void *)(e & ~0xFFF));
        continue;
      }

      free_p2((off_t *)(e & ~0xFFF));
    }
  }

  phys::free(p3_p);
}

void x86::free_table(void *cr3) {
  off_t *pml4 = (off_t *)p2v(cr3);
  // only loop over the lower half (userspace)
  for (int i = 0; i < 272; i++) {
    if (pml4[i]) {
      free_p3((off_t *)(pml4[i] & ~0xFFF));
    }
  }

  phys::free(cr3);
}
