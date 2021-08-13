#pragma once

typedef __INT8_TYPE__ i8;
typedef __INT8_TYPE__ int8_t;
typedef __UINT8_TYPE__ u8;

typedef __INT16_TYPE__ i16;
typedef __INT16_TYPE__ int16_t;
typedef __UINT16_TYPE__ u16;

typedef __INT32_TYPE__ i32;
typedef __UINT32_TYPE__ u32;


typedef __INT32_TYPE__ int32_t;


typedef __INT64_TYPE__ i64;
typedef __INT64_TYPE__ int64_t;
typedef __UINT64_TYPE__ u64;

typedef __SIZE_TYPE__ size_t;
typedef long ssize_t;
typedef size_t off_t;

// typedef unsigned long long uint64_t;
typedef unsigned long uint64_t;

typedef unsigned int uint32_t;
typedef int sint32_t;

typedef unsigned short uint16_t;
typedef short sint16_t;

typedef unsigned char uint8_t;
typedef char sint8_t;

typedef long time_t;

struct tm {
  int tm_sec;    //	int	seconds after the minute	0-61*
  int tm_min;    // int	minutes after the hour	0-59
  int tm_hour;   // int	hours since midnight	0-23
  int tm_mday;   // int	day of the month	1-31
  int tm_mon;    // int	months since January	0-11
  int tm_year;   // int	years since 1900
  int tm_wday;   // int	days since Sunday	0-6
  int tm_yday;   // int	days since January 1	0-365
  int tm_isdst;  // int	Daylight Saving Time flag	};
};

typedef u64 addr_t;

// #ifndef __cplusplus
// typedef u8 bool;
// #endif

typedef unsigned long uintptr_t;
typedef long intptr_t;

#ifndef NULL
#define NULL 0
#endif
#define FALSE ((bool)0)
#define TRUE ((bool)1)

#ifndef true
#define true TRUE
#endif

#ifndef false
#define false FALSE
#endif

#define PGSIZE 4096
