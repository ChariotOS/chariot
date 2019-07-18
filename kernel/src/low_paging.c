#include <asm.h>
#include <types.h>

/**
 * This file contains paging utilities for setting up paging to get things working.
 * It basically just does enough to identity map the kernel code.
 *
 */


/*
// pg_heap is a region of memory that is setup by the linker which contains
// 16 contiguious pages.
extern char pg_heap;
// how many pages in that heap have been used? The memory allocator here is not smart
static int pg_heap_used = 0;
static u64 *p4_table = NULL;

static u64 *palloc(void) BOOTCODE;
u64 *low_id_map_code(void) BOOTCODE;


u64 *low_id_map_code(void) {
  return p4_table;
}
*/
