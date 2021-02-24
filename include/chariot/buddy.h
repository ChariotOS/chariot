#pragma once

#include <list_head.h>
#include <lock.h>

struct buddy_mempool {
  unsigned long base_addr;  /** base address (virtual) of the memory pool */
  unsigned long pool_order; /** size of memory pool = 2^pool_order */
  unsigned long min_order;  /** minimum allocatable block size */

  unsigned long num_blocks; /** number of bits in tag_bits */
  unsigned long *tag_bits;  /** one bit for each 2^min_order block
                             *   0 = block is allocated
                             *   1 = block is available
                             */

  struct list_head *avail; /** one free list for each block size,
                            * indexed by block order:
                            *   avail[i] = free list of 2^i blocks
                            */

  spinlock lock;


  buddy_mempool(unsigned long base_addr, unsigned long pool_order, unsigned long min_order);

  void free(void *addr, unsigned long order);
  void *malloc(unsigned long order);
};
