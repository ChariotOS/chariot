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

#include <hawkbeans.h>
#include <class.h>
#include <types.h>


#include <stdint.h>

/* 
 * KCH: this file contains various utility functions used throughout, e.g. bit
 * operations and integer math. A good deal of this is taken from the Linux
 * kernel.
 */

extern int ____ilog2_NaN(void);

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))

/* 
 * KCH: this is modified from its original version to use clz, as
 * there is no builtin fls provided by gcc 
 */
static inline __attribute__((const)) int 
__ilog2_u32 (uint32_t n)
{
	return (8*sizeof(unsigned) - __builtin_clz(n) - 1);
}

/* 
 * KCH: this is modified from its original version to use clz, as
 * there is no builtin fls provided by gcc 
 */
static inline __attribute__((const)) int 
__ilog2_u64 (uint64_t n)
{
	return (8*sizeof(unsigned long long) - __builtin_clzll(n) - 1);
}

static inline int 
fls (int x)
{
	int r;

	/*
	 * AMD64 says BSRL won't clobber the dest reg if x==0; Intel64 says the
	 * dest reg is undefined if x==0, but their CPU architect says its
	 * value is written to set it to the same as before, except that the
	 * top 32 bits will be cleared.
	 *
	 * We cannot do this on 32 bits because at the very least some
	 * 486 CPUs did not behave this way.
	 */
	asm("bsrl %1,%0"
	    : "=r" (r)
	    : "rm" (x), "0" (-1));
	return r + 1;
}


/**
 * fls64 - find last set bit in a 64-bit word
 * @x: the word to search
 *
 * This is defined in a similar way as the libc and compiler builtin
 * ffsll, but returns the position of the most significant set bit.
 *
 * fls64(value) returns 0 if value is 0 or the position of the last
 * set bit if value is nonzero. The last (most significant) bit is
 * at position 64.
 */
static inline int 
fls64 (uint64_t x)
{
	long bitpos = -1;
	/*
	 * AMD64 says BSRQ won't clobber the dest reg if x==0; Intel64 says the
	 * dest reg is undefined if x==0, but their CPU architect says its
	 * value is written to set it to the same as before.
	 */
	asm("bsrq %1,%q0"
	    : "+r" (bitpos)
	    : "rm" (x));
	return bitpos + 1;
}


static inline unsigned 
fls_long (unsigned long l)
{
	if (sizeof(l)== 4) {
		return fls(l);
	} 
	return fls64(l);
}


/**
 * roundup_pow_of_two - round the given value up to nearest power of two
 * @n - parameter
 *
 * round the given value up to the nearest power of two
 * - the result is undefined when n == 0
 * - this can be used to initialise global variables from constant data
 */
#define roundup_pow_of_two(n)           \
(                       \
    __builtin_constant_p(n) ? (     \
        (n == 1) ? 1 :          \
        (1UL << (ilog2((n) - 1) + 1))   \
                   ) :      \
    __roundup_pow_of_two(n)         \
 )

/*
 * round up to nearest power of two
 */
static inline __attribute__((const)) unsigned long 
__roundup_pow_of_two (unsigned long n)
{
    return 1UL << fls_long(n - 1);
}

#ifndef ilog2
#define ilog2(n)				\
(						\
	__builtin_constant_p(n) ? (		\
		(n) < 1 ? ____ilog2_NaN() :	\
		(n) & (1ULL << 63) ? 63 :	\
		(n) & (1ULL << 62) ? 62 :	\
		(n) & (1ULL << 61) ? 61 :	\
		(n) & (1ULL << 60) ? 60 :	\
		(n) & (1ULL << 59) ? 59 :	\
		(n) & (1ULL << 58) ? 58 :	\
		(n) & (1ULL << 57) ? 57 :	\
		(n) & (1ULL << 56) ? 56 :	\
		(n) & (1ULL << 55) ? 55 :	\
		(n) & (1ULL << 54) ? 54 :	\
		(n) & (1ULL << 53) ? 53 :	\
		(n) & (1ULL << 52) ? 52 :	\
		(n) & (1ULL << 51) ? 51 :	\
		(n) & (1ULL << 50) ? 50 :	\
		(n) & (1ULL << 49) ? 49 :	\
		(n) & (1ULL << 48) ? 48 :	\
		(n) & (1ULL << 47) ? 47 :	\
		(n) & (1ULL << 46) ? 46 :	\
		(n) & (1ULL << 45) ? 45 :	\
		(n) & (1ULL << 44) ? 44 :	\
		(n) & (1ULL << 43) ? 43 :	\
		(n) & (1ULL << 42) ? 42 :	\
		(n) & (1ULL << 41) ? 41 :	\
		(n) & (1ULL << 40) ? 40 :	\
		(n) & (1ULL << 39) ? 39 :	\
		(n) & (1ULL << 38) ? 38 :	\
		(n) & (1ULL << 37) ? 37 :	\
		(n) & (1ULL << 36) ? 36 :	\
		(n) & (1ULL << 35) ? 35 :	\
		(n) & (1ULL << 34) ? 34 :	\
		(n) & (1ULL << 33) ? 33 :	\
		(n) & (1ULL << 32) ? 32 :	\
		(n) & (1ULL << 31) ? 31 :	\
		(n) & (1ULL << 30) ? 30 :	\
		(n) & (1ULL << 29) ? 29 :	\
		(n) & (1ULL << 28) ? 28 :	\
		(n) & (1ULL << 27) ? 27 :	\
		(n) & (1ULL << 26) ? 26 :	\
		(n) & (1ULL << 25) ? 25 :	\
		(n) & (1ULL << 24) ? 24 :	\
		(n) & (1ULL << 23) ? 23 :	\
		(n) & (1ULL << 22) ? 22 :	\
		(n) & (1ULL << 21) ? 21 :	\
		(n) & (1ULL << 20) ? 20 :	\
		(n) & (1ULL << 19) ? 19 :	\
		(n) & (1ULL << 18) ? 18 :	\
		(n) & (1ULL << 17) ? 17 :	\
		(n) & (1ULL << 16) ? 16 :	\
		(n) & (1ULL << 15) ? 15 :	\
		(n) & (1ULL << 14) ? 14 :	\
		(n) & (1ULL << 13) ? 13 :	\
		(n) & (1ULL << 12) ? 12 :	\
		(n) & (1ULL << 11) ? 11 :	\
		(n) & (1ULL << 10) ? 10 :	\
		(n) & (1ULL <<  9) ?  9 :	\
		(n) & (1ULL <<  8) ?  8 :	\
		(n) & (1ULL <<  7) ?  7 :	\
		(n) & (1ULL <<  6) ?  6 :	\
		(n) & (1ULL <<  5) ?  5 :	\
		(n) & (1ULL <<  4) ?  4 :	\
		(n) & (1ULL <<  3) ?  3 :	\
		(n) & (1ULL <<  2) ?  2 :	\
		(n) & (1ULL <<  1) ?  1 :	\
		(n) & (1ULL <<  0) ?  0 :	\
		____ilog2_NaN()			\
				   ) :		\
	(sizeof(n) <= 4) ?			\
	__ilog2_u32(n) :			\
	__ilog2_u64(n)				\
 )
#endif

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_PER_BYTE  8
#define BITS_PER_INT  (sizeof(int)*BITS_PER_BYTE)
#define BITS_PER_LONG (sizeof(long)*BITS_PER_BYTE)

#define BIT(nr)			(1UL << (nr))
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define min(x, y) ({                \
            typeof(x) _min1 = (x);          \
            typeof(y) _min2 = (y);          \
            (void) (&_min1 == (void*)&_min2);      \
            _min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({                \
            typeof(x) _max1 = (x);          \
            typeof(y) _max2 = (y);          \
            (void) (&_max1 == (void*)&_max2);      \
            _max1 > _max2 ? _max1 : _max2; })

#define min3(x, y, z) ({            \
            typeof(x) _min1 = (x);          \
            typeof(y) _min2 = (y);          \
            typeof(z) _min3 = (z);          \
            (void) (&_min1 == (void*)&_min2);      \
            (void) (&_min1 == (void*)&_min3);      \
            _min1 < _min2 ? (_min1 < _min3 ? _min1 : _min3) : \
                (_min2 < _min3 ? _min2 : _min3); })

#define max3(x, y, z) ({            \
            typeof(x) _max1 = (x);          \
            typeof(y) _max2 = (y);          \
            typeof(z) _max3 = (z);          \
            (void) (&_max1 == (void*)&_max2);      \
            (void) (&_max1 == (void*)&_max3);      \
            _max1 > _max2 ? (_max1 > _max3 ? _max1 : _max3) : \
                (_max2 > _max3 ? _max2 : _max3); })



#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size));		\
	     (bit) < (size);					\
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

/* same as for_each_set_bit() but use bit as value to start with */
#define for_each_set_bit_from(bit, addr, size) \
	for ((bit) = find_next_bit((addr), (size), (bit));	\
	     (bit) < (size);					\
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

#define for_each_clear_bit(bit, addr, size) \
	for ((bit) = find_first_zero_bit((addr), (size));	\
	     (bit) < (size);					\
	     (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

/* same as for_each_clear_bit() but use bit as value to start with */
#define for_each_clear_bit_from(bit, addr, size) \
	for ((bit) = find_next_zero_bit((addr), (size), (bit));	\
	     (bit) < (size);					\
	     (bit) = find_next_zero_bit((addr), (size), (bit) + 1))


/* BITMAP FUNCTIONS */

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG)

static inline void 
bitmap_zero (unsigned long *dst, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = 0UL;
	else {
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memset(dst, 0, len);
	}
}

static inline int 
test_bit (unsigned int nr, const volatile unsigned long *addr)
{
    return ((1UL << (nr % BITS_PER_LONG)) &
        (addr[nr / BITS_PER_LONG])) != 0;
}

static void
hb_val_from_type (char * str, void * val, u2 desc_idx, java_class_t * cls)
{
    const char * desc = hb_get_const_str(desc_idx, cls);

    if (!desc) {
        sprintf(str, "???");
        return;
    }

    switch (*desc) {
        case 'B':
            sprintf(str, "%d", (char) (uint64_t)val);
            break;
        case 'C':
            sprintf(str, "%c", (u1) (uint64_t)val);
            break;
        case 'D':
            sprintf(str, "%f", (d8)(u8) (uint64_t)val);
            break;
        case 'F':
            sprintf(str, "%f", (f4)(u4) (uint64_t)val);
            break;
        case 'I':
            sprintf(str, "%d", (int) (uint64_t)val);
            break;
        case 'J':
            sprintf(str, "%ld", (u8) (uint64_t)val);
            break;
        case 'L':
            sprintf(str, "(ref) %p",  (uint64_t)val);
            break;
        case 'S':
            sprintf(str, "%d", (u2) (uint64_t)val);
            break;
        case 'Z':
            sprintf(str, "%s", ((u1) (uint64_t)val) == true ? "true" : "false");
            break;
        case '[':
            sprintf(str, "(array) %p", val);
            break;
        default:
            HB_ERR("Unkown type (%c)", *desc);
            return;
    }
}
