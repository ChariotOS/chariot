#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

static unsigned long read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}



int main(int argc, char **argv) {
	for (int i = 0; i < 100; i++) {
		auto start = read_timestamp();
		dup(0);
		auto end = read_timestamp();
		printf("%lu\n", end-start);
	}
	return 0;
}
