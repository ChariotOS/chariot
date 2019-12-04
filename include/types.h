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

typedef long time_t;

struct tm {
  int tm_sec;     //	int	seconds after the minute	0-61*
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

#ifndef __cplusplus
typedef u8 bool;
#endif

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

#endif
