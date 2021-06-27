#include <ck/time.h>
#include <sys/sysbind.h>

uint64_t ck::time::us(void) { return sysbind_gettime_microsecond(); }