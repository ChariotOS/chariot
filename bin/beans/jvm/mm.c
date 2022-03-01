/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2019, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <hb_util.h>
#include <exceptions.h>
#include <class.h>
#include <list.h>
#include <gc.h>
#include <mm.h>


/* TODO: separate arch specific (mmap) */


struct heap_info * heap;

/*
 * Initializes the JVM heap. the heap will
 * be mapped anonymously and is required to be
 * a power of two. This heap size is currently
 * determined statically at compile-time. 
 *
 * @return: 0 on success, -1 otherwise.
 * 
 */
int
heap_init (int heap_size_megs)
{
	void * heap_ptr = NULL;
	int i;
	int size = heap_size_megs ? (heap_size_megs*1024*1024) : HB_DEFAULT_HEAP_SIZE;

	heap_ptr = mmap(NULL,
			roundup_pow_of_two(size),
			PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS,
			-1,
			0);

	if (heap_ptr == MAP_FAILED) {
		HB_ERR("Could not allocate JVM heap\n");
		return -1;
	}
	     
	MM_DEBUG("Allocated %d.%d MB of heap space\n", size/1000000, size%1000000);

	heap = malloc(sizeof(struct heap_info));
	if (!heap) {
		HB_ERR("Could not allocate heap structure\n");
		return -1;
	}
	memset(heap, 0, sizeof(struct heap_info));
	
	heap->heap_region = heap_ptr;
	heap->obj_count   = 0;
	heap->order       = ilog2(roundup_pow_of_two(size));
	heap->allocated   = (1UL << heap->order);
	heap->min_order   = ilog2(roundup_pow_of_two(sizeof(native_obj_t)));

	heap->free_lists =  malloc((heap->order + 1) * sizeof(struct list_head));

	if (!heap->free_lists) {
		HB_ERR("Could not allocate list heads\n");
		return -1;
	}

	/* all free lists are initially empty */
	for (i = 0; i <= heap->order; i++) {
		INIT_LIST_HEAD(&(heap->free_lists[i]));
	}

	/* bitmap for minimum sized chunks */
	heap->num_min_blocks = (1 << heap->order) / (1 << heap->min_order);
	heap->tag_bits       = malloc(BITS_TO_LONGS(heap->num_min_blocks) * sizeof(long));

	if (!heap->tag_bits) {
		HB_ERR("Could not allocate tag bits\n");
		return -1;
	}

	BUDDY_DEBUG("buddy allocator: num_blocks=%lu, tag_bits=%p, alloc=%lu\n", 
		  heap->num_min_blocks, heap->tag_bits, BITS_TO_LONGS(heap->num_min_blocks)*sizeof(long));

	/* mark all min blocks as allocated */
	bitmap_zero((unsigned long *)heap->tag_bits, heap->num_min_blocks);

	/* now we free them up. free will automatically coalesce adjacent blocks */
	u8 addr = (u8)heap->heap_region;
	for (i = 0; i < heap->num_min_blocks; i++) {
		buddy_free((void*)addr, heap->min_order);
		addr += (1 << heap->min_order);
	}

	return 0;
}


/*
 * Allocates an array of the given type, of 
 * length given by count. The fact that its an array
 * is given by the flags in the native object structure.
 *
 * @return: a reference to the array object on success,
 * NULL otherwise.
 *
 */
obj_ref_t *
array_alloc (u1 type, i4 count)
{
	obj_ref_t * ref = NULL;
	native_obj_t * obj = NULL;
	int sz;

	MM_DEBUG("Allocating array of type %d length %d\n", type, count);
	ref = malloc(sizeof(obj_ref_t));
	if (!ref) {
		HB_ERR("Could not allocate object ref\n");
		return NULL;
	}
	memset(ref, 0, sizeof(obj_ref_t));

	sz = sizeof(native_obj_t) + (sizeof(var_t)*(count+1));
	
	obj = alloc_checked(sz);

	if (!obj) {
		HB_ERR("THROWING OUT OF MEMORY EXCEPTION in %s\n", __func__);
		// throw_exception();
		return HB_NULL;
	}

	obj->fields = (var_t*)((u8)obj + sizeof(native_obj_t));
	obj->field_infos = NULL;
	obj->field_count = count;

	// set all entries to zero by default
	memset(obj->fields, 0, sizeof(var_t)*(count+1));

	// specify this is an array
	obj->flags.array.isarray = 1;
	obj->flags.array.type    = type;
	obj->flags.array.length  = count;

	// TODO: This should return a built-in array class
	obj->class             = g_java_object_class;

	ref->heap_ptr = (u8)obj;
	ref->type     = OBJ_ARRAY;
	
	return ref;
}


/*
 * Allocates an object of Class String.
 *
 * @return: the new object of Class String. NULL otherwise.
 *
 * Will be called from gc
 *
 */
obj_ref_t * 
string_object_alloc (const char * str)
{
	obj_ref_t * ref = NULL;
	native_obj_t * obj = NULL;
	obj_ref_t * arr_ref = NULL;
	native_obj_t * arr = NULL;
	java_class_t * cls = hb_get_or_load_class("java/lang/String");
	int i;

	if (!cls) { 
		HB_ERR("Could not get string class\n");
		return NULL;
	}

	ref = object_alloc(cls);
	if (!ref) {
		HB_ERR("Could not allocate string object\n");
		return NULL;
	}

	obj = (native_obj_t*)ref->heap_ptr;

	// note we don't create room for the null terminator
	arr_ref = gc_array_alloc(T_CHAR, strlen(str));

	if (!arr_ref) {
		HB_ERR("Could not allocate character array for String object\n");
		return NULL;
	}

	arr = (native_obj_t*)arr_ref->heap_ptr;

	for (i = 0; i < strlen(str); i++) {
		arr->fields[i].char_val = str[i];
	}
	
	obj->fields[0].obj = arr_ref;

	MM_DEBUG("String object allocated at %p (%s)\n", ref, str);

	return ref;
}



/*
 * Allocates an object for the given class
 *
 * @return: a pointer to an object reference on
 * success, NULL otherwise.
 *
 * TODO: instance vars should be set to their default values
 * based on constant pool
 *
 * Will be called from GC
 *
 */
obj_ref_t * 
object_alloc (java_class_t * cls)
{
	obj_ref_t * ref = NULL;
	native_obj_t * obj = NULL;
	int sz;
	int field_count;

	ref = malloc(sizeof(obj_ref_t));
	if (!ref) {
        hb_throw_and_create_excp(EXCP_OOM);
		return NULL;
	}
	memset(ref, 0, sizeof(obj_ref_t));

	field_count = hb_get_obj_field_count(cls);

	sz = sizeof(native_obj_t) + (sizeof(var_t)*field_count) + (sizeof(field_info_t*)*field_count);

	obj = alloc_checked(sz);

	if (!obj) {
		hb_throw_and_create_excp(EXCP_OOM);
		return HB_NULL;
	}

	memset(obj, 0, sz);

	obj->class       = cls;
	obj->field_count = field_count;
	obj->fields      = (var_t*)((u8)obj + sizeof(native_obj_t));
	obj->field_infos = (field_info_t**)((u8)obj->fields + (sizeof(var_t)*field_count));

	if (hb_setup_obj_fields(obj, obj->class) < 0) {
		HB_ERR("Could not setup fields for new object in class %s\n", 
                hb_get_class_name(obj->class));
		return HB_NULL;
	}

	ref->heap_ptr = (u8)obj;
	ref->type     = OBJ_OBJ;
	
	return ref;
}


void
object_free (native_obj_t * obj) {
	buddy_free((void*)obj, obj->order);
}


/*
 * Allocates an object with the given size.
 * The size will be rounded up to the nearest power of 2
 * so as to be amenable to the buddy allocator. 
 *
 * @return: a pointer to a native object structure on success,
 * NULL otherwise.
 *
 */
native_obj_t *
alloc_checked (const u4 size)
{
	native_obj_t * obj = NULL;
	u2 order = ilog2(roundup_pow_of_two(size));

	MM_DEBUG("Allocating size %u (rounded up to %lu)\n", size, 1UL<<order);

	obj = (native_obj_t*)buddy_alloc(order);

	if (!obj) {
		return NULL;
	}

	obj->flags.val = 0;
	obj->order     = order;

	return obj;
}


/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void 
__set_bit (u8 nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "btsq %1,%0"
        :"+m" (*(volatile long*)addr)
        :"r" (nr) : "memory");
}


static inline void 
__clear_bit (u8 nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "btrq %1,%0"
        :"+m" (*(volatile long*)addr)
        :"r" (nr));
}

static inline void 
setb (u8 nr, volatile char *addr)
{
    u8 offset, bitoffset;
    offset = nr/8;
    bitoffset = nr % 8;

    addr[offset] |= (0x1UL << bitoffset);
}

static inline void 
clearb (u8 nr, volatile char *addr)
{
    u8 offset, bitoffset;
    offset = nr/8;
    bitoffset = nr % 8;

    addr[offset] &= ~(0x1UL << bitoffset);
}

/**
 * Converts a block address to its block index in the specified buddy allocator.
 * A block's index is used to find the block's tag bit, mp->tag_bits[block_id].
 */
static inline u8
block_to_id (native_obj_t *block)
{
    u8 block_id =
        ((u8)block - (u8)heap->heap_region) >> heap->min_order;

    return block_id;
}


/**
 * Marks a block as free by setting its tag bit to one.
 */
static inline void
mark_available (native_obj_t *blk)
{
    if (blk == (native_obj_t*)0xdfa00000ULL) {
	BUDDY_DEBUG("Magic block %p: block_to_id=%lu\n", blk, block_to_id(blk));
    }

    __set_bit(block_to_id(blk), (volatile char*)heap->tag_bits);
}


/**
 * Marks a block as allocated by setting its tag bit to zero.
 */
static inline void
mark_allocated (native_obj_t *blk)
{
    __clear_bit(block_to_id(blk), (volatile char *)heap->tag_bits);
}


/**
 * Returns true if block is free, false if it is allocated.
 */
static inline int
is_available (native_obj_t *blk)
{
    return test_bit(block_to_id(blk), (const volatile unsigned long *)heap->tag_bits);
}


/**
 * Returns the address of the block's buddy block.
 */
static void *
find_buddy (native_obj_t *blk, u2 order)
{
    u8 _block;
    u8 _buddy;

    /* Fixup block address to be zero-relative */
    _block = (u8)blk - (u8)heap->heap_region;

    /* Calculate buddy in zero-relative space */
    _buddy = _block ^ (1UL << order);

    /* Return the buddy's address */
    return (void *)(_buddy + (u8)heap->heap_region);
}


void *
buddy_alloc (u2 order)
{
    u2 j;
    struct list_head *list;
    native_obj_t *blk;
    native_obj_t *buddy_blk;

    BUDDY_DEBUG("BUDDY ALLOC order: %u\n", order);

    if (order > heap->order) {
	BUDDY_DEBUG("order is too big\n");
        return NULL;
    }

    /* Fixup requested order to be at least the minimum supported */
    if (order < heap->min_order) {
        order = heap->min_order;
	BUDDY_DEBUG("order expanded to %u\n",order);
    }

    for (j = order; j <= heap->order; j++) {

        /* Try to allocate the first block in the order j list */
        list = &heap->free_lists[j];

        if (list_empty(list)) {
	    BUDDY_DEBUG("Skipping order %u as the list is empty\n", j);
            continue;
        }

        blk = list_entry(list->next, native_obj_t, link);
        list_del_init(&blk->link);
        mark_allocated(blk);

	BUDDY_DEBUG("Found block %p at order %u\n", blk, j);

        /* Trim if a higher order block than necessary was allocated */
        while (j > order) {
            --j;
            buddy_blk = (native_obj_t*)((u8)blk + (1UL << j));
            buddy_blk->order = j;
            mark_available(buddy_blk);
	    BUDDY_DEBUG("Inserted buddy block %p into order %u\n", buddy_blk, j);
            list_add(&buddy_blk->link, &heap->free_lists[j]);
        }

	blk->order = j;

	BUDDY_DEBUG("Returning block %p (order=%u)\n", blk, blk->order);

	heap->allocated += (1UL << blk->order);

        return blk;
    }

    BUDDY_DEBUG("FAILED TO ALLOCATE RETURNING NULL\n");

    return NULL;
}

void
buddy_free (void * addr, u2 order)
{
	native_obj_t * blk = NULL;

	BUDDY_DEBUG("BUDDY FREE on addr=%p, order=%u\n", addr, order);

	if (order < heap->min_order) {
		order = heap->min_order;
		BUDDY_DEBUG("order updated to %u\n", heap->min_order);
	}

	if (order > heap->order) {
		HB_ERR("Tried to free block bigger than heap!\n");
		return;
	}

	blk = (native_obj_t*)addr;

	/* TODO: make sure this is not an already free block */

	heap->allocated -= (1UL << order);

	/* coalescing stage */
	while (order < heap->order) {
		native_obj_t * buddy = find_buddy(blk, order);
		BUDDY_DEBUG("buddy at order %u is %p\n", order, buddy);
		
		if (!is_available(buddy)) {
			BUDDY_DEBUG("buddy not available\n");
			break;
		}

		if (is_available(buddy) && (buddy->order != order)) {
			BUDDY_DEBUG("buddy available but has order %u\n", buddy->order);
			break;
		}

		BUDDY_DEBUG("Buddy merge\n");
		
		list_del_init(&(buddy->link));
		if (buddy < blk) {
			blk = buddy;
		}
		blk->order = ++order;
	}

	blk->order = order;

	BUDDY_DEBUG("End of search: block=%p order=%u heap order=%u block->order=%u\n",
			blk, order, heap->order, blk->order);

	mark_available(blk);

	list_add(&(blk->link), &(heap->free_lists[order]));
	
	if (blk->order == -1) {
		HB_ERR("FAIL: block order went nuts\n");
	}
}


void 
buddy_stats (void)
{
	HB_INFO("HEAP ADDR: %p\n", heap->heap_region);
	HB_INFO("HEAP SIZE: %luB\n", (1UL<<heap->order));
	HB_INFO("HEAP ALLOC: %luB\n", heap->allocated);
	HB_INFO("HEAP FREE: %luB\n", (1UL<<heap->order) - heap->allocated);
	HB_INFO("HEAP MIN ORDER: %u (%lu B)\n", heap->min_order, (1UL<<heap->min_order));
	HB_INFO("HEAP #MIN BLKS: %lu\n", heap->num_min_blocks);
}

