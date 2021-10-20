
#include <ck/time.h>
#include <sys/sysbind.h>
#include <stdio.h>
#include <unistd.h>
#include <chariot.h>

uint64_t ck::time::us(void) { return sysbind_gettime_microsecond(); }

// uint64_t ck::time::cycles(void) {
// #ifdef CONFIG_X86
//   uint32_t lo, hi;
//   /* TODO; */
//   asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
//   return lo | ((uint64_t)(hi) << 32);
// #endif

//   panic("ck::time::cycles unimplemented for this arch.");

//   return 0;
// }