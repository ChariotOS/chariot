#pragma once
/*
 * This file might feel strange, but it exists for a good reason (I promise).
 * It implements
 */
#if 0
#include <riscv/arch.h>

#ifdef PAGING_IMPL_BOOTCODE

#define _paging_code static __init
extern "C" char _kernel_end[];
static int boot_pages_allocated = 0;
_paging_code rv::xsize_t *alloc_pages(int n) {
  auto p = (rv::xsize_t *)(_kernel_end + (boot_pages_allocated) * (4096 * n));
  boot_pages_allocated += n;
  return p;
}

#define page_v2p(n) ((rv::xsize_t *)(n))
#define page_p2v(n) ((rv::xsize_t *)(n))

#else

#define _paging_code static
#include <phys.h>
#define alloc_pages(n) (phys::alloc((n)))

#define page_v2p(n) ((rv::xsize_t *)v2p(n))
#define page_p2v(n) ((rv::xsize_t *)p2v(n))

#endif



#ifndef CONFIG_RISCV
#error "paging_impl.h only works on riscv."
#endif

/*
 * VM_BITS      - the overall bits (typically XX in SVXX)
 * VM_PART_BITS - how many bits per VPN[x]
 * VM_PART_NUM  - how many parts are there?
 */

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

#define OFFSET_BITS (12)

#define VM_BITS ((VM_PART_BITS * VM_PART_NUM) + OFFSET_BITS)




#define PT_V (1 << 0)
#define PT_R (1 << 1)
#define PT_W (1 << 2)
#define PT_X (1 << 3)
#define PT_U (1 << 4)
#define PT_G (1 << 5)
#define PT_A (1 << 6)
#define PT_D (1 << 7)

#define PTE(PHYS_ADDR, OPT) (((((rv::xsize_t)(PHYS_ADDR)) >> 12) << 9) | (OPT))



_paging_code bool rv_map_page(rv::xsize_t *page_table, off_t ppn, int prot) {
	return false;	
}
#endif
