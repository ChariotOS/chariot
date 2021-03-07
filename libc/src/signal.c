#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>


int kill(pid_t pid, int sig) {
  return errno_wrap(sysbind_kill(pid, sig));
}

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
  if (sig < 0 || sig > 32) {
    errno = EINVAL;
    return -1;
  }
  *set |= 1 << (sig);
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


  int r = errno_wrap(sysbind_sigprocmask(how, *set, &oldv));

  if (old != NULL) {
    *old = oldv;
  }

  return r;
}


int sigaction(int sig, struct sigaction *act, struct sigaction *old) {
  return errno_wrap(sysbind_sigaction(sig, act, old));
}

typedef void (*signal_handler_t)(int);

signal_handler_t signal(int sig, signal_handler_t handler) {
  struct sigaction new_act;
  struct sigaction old_act;
  new_act.sa_handler = handler;
  new_act.sa_flags = 0;
  new_act.sa_mask = 0;
  int rc = sigaction(sig, &new_act, &old_act);
  if (rc < 0) return SIG_ERR;

  return old_act.sa_handler;
}
