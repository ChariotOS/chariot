#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/sysbind.h>
#include <string.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: allocate <size in MB>\n");
    exit(EXIT_FAILURE);
  }

  size_t mb = 0;

  if (sscanf(argv[1], "%zu", &mb) == 0) {
    fprintf(stderr, "invalid number '%s'\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  printf("Allocating %zuMB of ram and writing 1 byte per page...\n", mb);

	uint64_t start = sysbind_gettime_microsecond();

  size_t size = round_up(mb * 1024 * 1024, 4096);

  for (off_t i = 0; i < size; i += 4096) {
		char *p = malloc(4096);
		p[0] = 0;

    // printf("\r%.2f%%", (float)i / size * 100.0);
  }

	uint64_t end = sysbind_gettime_microsecond();

  printf("\nDone in %lu ms!\n", (end - start) / 1000);



  return 0;
}
