#include <sys/mman.h>
#include <sys/syscall.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset) {
  long ptr = syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
  return (void*)ptr;
}
