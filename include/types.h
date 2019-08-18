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

typedef u64 size_t;
typedef i64 ssize_t;
typedef u64 off_t;

// typedef unsigned long long uint64_t;
typedef unsigned long uint64_t;

typedef unsigned int uint32_t;
typedef int sint32_t;

typedef unsigned short uint16_t;
typedef short sint16_t;

typedef unsigned char uint8_t;
typedef char sint8_t;

typedef u64 time_t;

typedef u64 addr_t;

#ifndef __cplusplus
typedef u8 bool;
#endif


typedef unsigned long uintptr_t;
typedef long intptr_t;

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



#define PGSIZE 4096


#endif
