#include <posix.h>
#include <stdarg.h>
#include <stddef.h>
#include <printk.h>

int stdin = 0;
int stdout = 1;
int stderr = 2;


// do_posix_syscall expects 6 ints and returns one int
extern int __posix_syscall(u64 args[6]);

int syscall(int n, ...) {
  va_list ap;
  u64 args[6];

  args[0] = n;
  va_start(ap, n);
  args[1] = va_arg(ap, u64);
  args[2] = va_arg(ap, u64);
  args[3] = va_arg(ap, u64);
  args[4] = va_arg(ap, u64);
  args[5] = va_arg(ap, u64);
  va_end(ap);
  return __posix_syscall(args);
}



u64 write(int fd, void *buf, u64 count) {
  return syscall(SYS_write, (u64)fd, (u64)buf, (u64)count);
}

int open(const char *filename, int flags, int opts) {
  return syscall(SYS_open, (u64)filename, (u64)flags, (u64)opts);
}
