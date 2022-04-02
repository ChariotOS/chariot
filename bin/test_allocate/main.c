#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/time.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

int main(int argc, char **argv) {
  size_t pages = 100000;
  if (argc == 2) {
    if (sscanf(argv[1], "%zu", &pages) == 0) {
      fprintf(stderr, "invalid number '%s'\n", argv[1]);
      exit(EXIT_FAILURE);
    }
  }



  // printf("Allocating %zuMB of ram and writing 1 byte per page...\n", mb);

  struct timeval start, stop;
  double secs = 0;

  gettimeofday(&start, NULL);

  size_t size = 4096 * pages;
  char *buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

  for (off_t i = 0; i < size; i += 4096) {
    buf[i] = 'a';
  }

  gettimeofday(&stop, NULL);
  secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);

  double us = secs * 1000.0 * 1000.0;

  printf("Done in %f us, %fs\n", us, secs);
  printf("time per fault: %f us\n", us / (float)pages);


  return 0;
}
