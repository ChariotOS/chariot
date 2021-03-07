#include <chariot/futex.h>
#include <ck/futex.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>

static int _futex(int *uaddr, int futex_op, int val, uint32_t timeout, int *uaddr2, int val3) {
  int r = 0;
  do {
    r = errno_wrap(sysbind_futex(uaddr, futex_op, val, 0, uaddr2, val3));
  } while (errno == EINTR);
  return r;
}

ck::futex::futex() : uaddr(0) {
}

ck::futex::futex(int i) : uaddr(i) {
}


void ck::futex::wait_on(int val) {
  while (1) {
    int futex_rc = _futex((int *)&uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
    if (futex_rc == -1) {
      if (errno != EAGAIN) {
        printf("error: %s\n", strerror(errno));
        while (1) {
        }
        exit(1);
      }
    } else if (futex_rc == 0) {
      if (uaddr == val) {
        // This is a real wakeup.
        return;
      }
    } else {
      abort();
    }
  }
}

void ck::futex::set(int val) {
  /* TODO: atomic? */
  uaddr = val;

  while (1) {
    int futex_rc = _futex((int *)&uaddr, FUTEX_WAKE, 1, NULL, NULL, 0);
    if (futex_rc == -1) {
      perror("futex wake");
      exit(1);
    } else if (futex_rc > 0) {
      return;
    }
  }
}
