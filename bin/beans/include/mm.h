/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2017, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#pragma once

#include <types.h>
#include <hawkbeans.h>

#if DEBUG_MM == 1
#define MM_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define MM_DEBUG(fmt, args...)
#endif

#if DEBUG_BUDDY == 1
#define BUDDY_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define BUDDY_DEBUG(fmt, args...)
#endif

/* linux */
#define HB_DEFAULT_HEAP_SIZE (1024*1024)

struct heap_info {
	void * heap_region;

	u8 allocated;

	u2 order; // log2(size of heap region)
	u2 min_order; // minimum sized block that can be allocated
	u8 num_min_blocks; // number of min blocks
	u8 obj_count; // number of allocated objects
	struct list_head * free_lists; // power of 2 free lists

	u8 * tag_bits; // bitmap for min blocks
};

struct java_class;

int heap_init(int heap_size_megs);

struct obj_ref * array_alloc(u1 type, i4 count);
struct obj_ref * string_object_alloc(const char * str);
struct obj_ref * object_alloc(struct java_class * cls);
struct native_object * alloc_checked(const u4 size);
void object_free(struct native_object * obj);
void * buddy_alloc (u2 order);
void buddy_free (void * addr, u2 order);
void buddy_stats (void);

