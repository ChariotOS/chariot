#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>

// bindings for liballoc

int liballoc_lock() {
  return 0;
}
int liballoc_unlock() {
  return 0;
}


void *liballoc_alloc(size_t s) {
  printf("alloc size = %d\n", (int)s);

  void *p = mmap(NULL, s * 4096, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);


  return p;
}


int liballoc_free(void *buf, size_t sz) {
  munmap(buf, sz * 4096);
  return 0;
}
