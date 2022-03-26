#pragma once

#ifndef _TIME_H
#define _TIME_H

#ifdef __cplusplus
extern "C" {
#endif



#define __NEED_size_t
#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_struct_timespec

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
#define __NEED_clockid_t
#define __NEED_timer_t
#define __NEED_pid_t
#define __NEED_locale_t
#define __NEED_clock_t
#endif

#include <bits/alltypes.h>

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


time_t time(time_t*);
time_t getlocaltime(struct tm* tloc);  // nonstandard
struct tm* localtime(const time_t* timep);

// clock() returns milliseconds
#define CLOCKS_PER_SEC (1000.0)
clock_t clock(void);

int clock_gettime(int id, struct timespec* s);
int clock_getres (int clock_id, struct timespec *res);


time_t mktime(struct tm*);
struct tm* gmtime(const time_t* t);
struct tm* gmtime_r(const time_t* timep, struct tm* result);

size_t strftime(char* s, size_t max, const char* format, const struct tm*);
double difftime(time_t a, time_t b);


#define CLOCK_REALTIME_COARSE	(0)
#define CLOCK_REALTIME		(1)
#define CLOCK_PROCESS_CPUTIME_ID (2)
#define CLOCK_THREAD_CPUTIME_ID	 (3)
#define CLOCK_MONOTONIC (5)
#define CLOCK_MONOTONIC_RAW (5)
#define CLOCK_MONOTONIC_COARSE (6)
#define CLOCK_BOOTTIME (7)
#define CLOCK_REALTIME_ALARM (8)
#define CLOCK_BOOTTIME_ALARM (9)


#ifdef __cplusplus
}
#endif


#endif
