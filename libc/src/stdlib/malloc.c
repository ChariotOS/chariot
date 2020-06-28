#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

static int malloc_lock = 0;



static inline int arch_atomic_swap(volatile int* x, int v) {
  __asm__("xchg %0, %1" : "=r"(v), "=m"(*x) : "0"(v) : "memory");
  return v;
}

static inline void arch_atomic_store(volatile int* p, int x) {
  __asm__("movl %1, %0" : "=m"(*p) : "r"(x) : "memory");
}

// bindings for liballoc

int liballoc_lock() {
  while (arch_atomic_swap(&malloc_lock, 1)) {
  }
  return 0;
}


int liballoc_unlock() {
  arch_atomic_store(&malloc_lock, 0);
  return 0;
}

int region_id = 0;
void *liballoc_alloc(size_t s) {
	// printf("[%d] malloc %d\n", getpid(), s);
  void *p = mmap(NULL, s * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	char name[32];
	snprintf(name, 32, "[malloc #%d]", region_id++);
  mrename(p, name);
  return p;
}


int liballoc_free(void *buf, size_t sz) {
  munmap(buf, sz * 4096);
  return 0;
}
