#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define LOCK(n)
#define UNLOCK(n)
/* Ensure that at least 32 atexit handlers can be registered without malloc */
#define COUNT 32

static struct fl {
  struct fl *next;
  void (*f[COUNT])(void *);
  void *a[COUNT];
} builtin, *head;

static int slot;
// static volatile int lock[1];



typedef void (*func_ptr)(void);
extern func_ptr __fini_array_start[0], __fini_array_end[0];

static void call_global_destructors(void) {
  for (func_ptr *func = __fini_array_start; func != __fini_array_end; func++) {
    (*func)();
  }
}


void __funcs_on_exit() {
#if 0
  int n = mregions(NULL, 0);
  if (n > 0) {
    struct mmap_region *regions = calloc(n, sizeof(*regions));
    n = mregions(regions, n);

    for (int i = 0; i < n; i++) {
      printf("%012llx-%012llx %c%c%c %s\n", regions[i].off,
             regions[i].off + regions[i].len,
						 regions[i].prot & PROT_READ ? 'R' : '-',
						 regions[i].prot & PROT_WRITE ? 'W' : '-',
						 regions[i].prot & PROT_EXEC ? 'X' : '-',
						 regions[i].name);
    }
    free(regions);
  }
#endif



  void (*func)(void *), *arg;
  LOCK(lock);
  for (; head; head = head->next, slot = COUNT)
    while (slot-- > 0) {
      func = head->f[slot];
      arg = head->a[slot];
      UNLOCK(lock);
      func(arg);
      LOCK(lock);
    }

  call_global_destructors();
}

void __cxa_finalize(void *dso) {
}

int __cxa_atexit(void (*func)(void *), void *arg, void *dso) {
  LOCK(lock);

  /* Defer initialization of head so it can be in BSS */
  if (!head) head = &builtin;

  /* If the current function list is full, add a new one */
  if (slot == COUNT) {
    struct fl *new_fl = calloc(sizeof(struct fl), 1);
    if (!new_fl) {
      UNLOCK(lock);
      return -1;
    }
    new_fl->next = head;
    head = new_fl;
    slot = 0;
  }

  /* Append function to the list. */
  head->f[slot] = func;
  head->a[slot] = arg;
  slot++;

  UNLOCK(lock);
  return 0;
}

static void call(void *p) {
  ((void (*)(void))(uintptr_t)p)();
}

int atexit(void (*func)(void)) {
  return __cxa_atexit(call, (void *)(uintptr_t)func, 0);
}
