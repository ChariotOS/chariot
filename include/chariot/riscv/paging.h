#pragma once


#ifndef CONFIG_RISCV
#error "paging_impl.h only works on riscv."
#endif

#define PGSIZE 4096  // bytes per page
#define PGSHIFT 12   // bits of offset within a page

#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))



/*
 * VM_BITS      - the overall bits (typically XX in SVXX)
 * VM_PART_BITS - how many bits per VPN[x]
 * VM_PART_NUM  - how many parts are there?
 */

#define VM_NORMAL_PAGE (0x1000L)
#define VM_MEGA_PAGE   (VM_NORMAL_PAGE << 9)
#define VM_GIGA_PAGE   (VM_MEGA_PAGE << 9)
#define VM_TERA_PAGE   (VM_GIGA_PAGE << 9)


/* Riscv sv32 - 32 bit virtual memory on 64bit */
#ifdef CONFIG_SV32
#define VM_PART_BITS 10
#define VM_PART_NUM 2
#endif

/* Riscv sv39 - 39 bit virtual memory on 64bit */
#ifdef CONFIG_SV39
#define VM_PART_BITS 9
#define VM_PART_NUM 3
#endif

/* Riscv sv48 - 48 bit virtual memory on 64bit */
#ifdef CONFIG_SV48
#define VM_PART_BITS 9
#define VM_PART_NUM 4
#endif

#define VM_BIGGEST_PAGE (VM_NORMAL_PAGE << ((VM_PART_NUM - 1) * VM_PART_BITS))

#define VM_BITS ((VM_PART_BITS * VM_PART_NUM) + PGSHIFT)

#define PT_V (1 << 0)
#define PT_R (1 << 1)
#define PT_W (1 << 2)
#define PT_X (1 << 3)
#define PT_U (1 << 4)
#define PT_G (1 << 5)
#define PT_A (1 << 6)
#define PT_D (1 << 7)

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((rv::xsize_t)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte)&0x3FF)

#define MAKE_PTE(pa, flags) (PA2PTE(pa) | (flags))

// extract the three N-bit page table indices from a virtual address.
#define PXMASK ((1llu << VM_PART_BITS) - 1)  // N bits
#define PXSHIFT(level) (PGSHIFT + (VM_PART_BITS * (level)))
#define PX(level, va) ((((rv::xsize_t)(va)) >> PXSHIFT(level)) & PXMASK)


// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// SvXX, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << ((VM_PART_BITS * VM_PART_NUM) + 12 - 1))



#ifdef CONFIG_SV39
#define SATP_MODE (8L << 60)
#endif

#ifdef CONFIG_SV48
#define SATP_MODE (9L << 60)
#endif

#ifdef CONFIG_SV32
#define SATP_MODE (1L << 30)
#endif

#define MAKE_SATP(page_table) (SATP_MODE | (((rv::xsize_t)page_table) >> 12))


namespace rv {

  namespace paging {};
};  // namespace rv
