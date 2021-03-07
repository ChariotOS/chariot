#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

static int malloc_lock = 0;


// bindings for liballoc

int liballoc_lock() {
  while (__sync_lock_test_and_set(&malloc_lock, 1)) {
  }
  return 0;
}


int liballoc_unlock() {
  malloc_lock = 0;
  return 0;
}

int region_id = 0;
void *liballoc_alloc(size_t s) {
  void *p = mmap(NULL, s * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  char name[32];
  snprintf(name, 32, "[malloc #%d]", region_id++);
  mrename(p, name);
  // memset(p, 0x0, s * 4096);
  return p;
}


int liballoc_free(void *buf, size_t sz) {
  munmap(buf, sz * 4096);
  return 0;
}
