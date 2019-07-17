#ifndef __TYPES_H__
#define __TYPES_H__

typedef signed char i8;
typedef unsigned char u8;

typedef signed short i16;
typedef unsigned short u16;

typedef signed int i32;
typedef unsigned int u32;

typedef signed long i64;
typedef unsigned long u64;

typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned long off_t;

// typedef unsigned long long uint64_t;
typedef unsigned long uint64_t;

typedef unsigned int uint32_t;
typedef int sint32_t;

typedef unsigned short uint16_t;
typedef short sint16_t;

typedef unsigned char uint8_t;
typedef char sint8_t;

typedef u64 addr_t;
typedef u8 bool_t;

typedef unsigned long uintptr_t;
typedef long intptr_t;

//#define NULL ((void *)0)
#ifndef NULL
#define NULL 0
#endif
#define FALSE 0
#define TRUE 1

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#endif
