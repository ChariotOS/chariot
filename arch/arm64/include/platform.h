#pragma once

#include <types.h>


#define MMU_MAPPING_FLAG_DEVICE 1

struct mmu_region {
	off_t start;
	size_t size;
	int flags;
	const char *name;
};

/*
 * In ARM, there isn't a general way to find the memory map of the machine,
 * so we defer to a hard coded table provided by the platform
 */
extern struct mmu_region mmu_mappings[];


/* Defined by the selected platform */
extern void arm64_platform_init(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3);
