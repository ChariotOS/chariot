#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <stdlib.h>

// zero out the set
int sigemptyset(sigset_t *s) {
  *s = 0;
  return 0;
}

// fill the set
int sigfillset(sigset_t *s) {
  // cross platform. Though it is likely an unsigned long
  memset(s, 0xFF, sizeof(*s));
  return 0;
}

int sigaddset(sigset_t *set, int sig) {
  if (sig < 1 || sig > 32) {
    errno = EINVAL;
    return -1;
  }
  *set |= 1 << (sig - 1);
  return 0;
}

int sigdelset(sigset_t *set, int sig) {
  if (sig < 1 || sig > 32) {
    errno = EINVAL;
    return -1;
  }
  *set &= ~(1 << (sig - 1));
  return 0;
}

int sigismember(const sigset_t *set, int sig) {
  if (sig < 1 || sig > 32) {
    errno = EINVAL;
    return -1;
  }
  if (*set & (1 << (sig - 1))) return 1;
  return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *old) {
	sigset_t oldv;


	int r = errno_syscall(SYS_sigprocmask, how, *set, &oldv);

	if (old != NULL) {
		*old = oldv;
	}

	return r;
}

void (*signal(int sig, void (*func)(int)))(int) { return SIG_ERR; }


void __signal_return_callback(void) {
	// just forward to the sigreturn syscall (no libc binding)
	syscall(SYS_sigreturn);

	/* [does not return] */
}
