#include <sys/mman.h>
#include <sys/syscall.h>
#include <stdio.h>


int munmap(void *addr, size_t length) {
  return syscall(SYS_munmap, addr, length);
  return -1;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset) {
  return (void*)syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
}


int mrename(void *addr, char *name) {
  return syscall(SYS_mrename, addr, name);
}


int mgetname(void *addr, char *name, size_t len) {
	return syscall(SYS_mgetname, addr, name, len);
}

int mregions(struct mmap_region *list, int entries) {
	return syscall(SYS_mregions, list, entries);
}
