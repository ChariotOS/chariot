#include <stdarg.h>
#include <sys/syscall.h>


// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);

typedef unsigned long long u64;


int write(int fd, void *data, unsigned long len) {
  return syscall(6, fd, data, len);
}

void libc_start() {
  main(0, 0, 0);
  // TODO: exit!
  while (1) {
  }
}
