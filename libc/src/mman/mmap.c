#include <stdio.h>
#include <sys/mman.h>
#include <sys/sysbind.h>


int munmap(void *addr, size_t length) {
  return sysbind_munmap(addr, length);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  return (void *)sysbind_mmap(addr, length, prot, flags, fd, offset);
}


int mrename(void *addr, char *name) {
  return sysbind_mrename(addr, name);
}


int mgetname(void *addr, char *name, size_t len) {
  return sysbind_mgetname(addr, name, len);
}

int mregions(struct mmap_region *list, int entries) {
  return sysbind_mregions(list, entries);
}
