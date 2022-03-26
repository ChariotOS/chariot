#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>


static pthread_mutex_t malloc_lock = PTHREAD_MUTEX_INITIALIZER;
// bindings for liballoc

int liballoc_lock() {
	pthread_mutex_lock(&malloc_lock);
  return 0;
}


int liballoc_unlock() {
	pthread_mutex_unlock(&malloc_lock);
  return 0;
}

int region_id = 0;
void *liballoc_alloc(size_t s) {
  void *p = mmap(NULL, s * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  char name[32];
  snprintf(name, 32, "[malloc #%d]", region_id++);
  mrename(p, name);
  memset(p, 0x0, s * 4096);
  return p;
}


int liballoc_free(void *buf, size_t sz) {
  munmap(buf, sz * 4096);
  return 0;
}
