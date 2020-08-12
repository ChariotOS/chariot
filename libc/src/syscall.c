#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

extern long __syscall(int num, unsigned long long a, unsigned long long b,
                      unsigned long long c, unsigned long long d,
                      unsigned long long e, unsigned long long f);


extern long __syscall_eintr(int num, unsigned long long a, unsigned long long b,
                            unsigned long long c, unsigned long long d,
                            unsigned long long e, unsigned long long f) {
  long count = 0;
  long res = 0;
  do {  // Loop while the syscall was interrupted. This isn't cool but it's way
        // easier than restartable syscalls
    res = __syscall(num, a, b, c, d, e, f);
    count++;
    if (res == -EINTR) {
      printf("[pid:%d] syscall interrupted\n", getpid());
    }
  } while (res == -EINTR);
  return res;
}

long syscall(long num, ...) {
  va_list ap;
  unsigned long long a, b, c, d, e, f;
  va_start(ap, num);
  a = va_arg(ap, unsigned long long);
  b = va_arg(ap, unsigned long long);
  c = va_arg(ap, unsigned long long);
  d = va_arg(ap, unsigned long long);
  e = va_arg(ap, unsigned long long);
  f = va_arg(ap, unsigned long long);
  va_end(ap);

  return __syscall_eintr(num, a, b, c, d, e, f);
}

long errno_syscall(long num, ...) {
  va_list ap;
  unsigned long long a, b, c, d, e, f;
  va_start(ap, num);
  a = va_arg(ap, unsigned long long);
  b = va_arg(ap, unsigned long long);
  c = va_arg(ap, unsigned long long);
  d = va_arg(ap, unsigned long long);
  e = va_arg(ap, unsigned long long);
  f = va_arg(ap, unsigned long long);
  va_end(ap);

  long result = __syscall_ret(__syscall_eintr(num, a, b, c, d, e, f));
  return result;
}

long __syscall_ret(long r) {
  if (r < 0) {
    errno = -r;
    return -1;
  }
  errno = 0;
  return r;
}
