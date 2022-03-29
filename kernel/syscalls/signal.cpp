#include <cpu.h>
#include <sched.h>
#include <sig.h>
#include <syscall.h>

int sys::signal_init(void *sigret) {
  // validate that the trampoline is valid
  // (currently mapped, as to avoid segfaults)
  if (!curproc->mm->validate_pointer(sigret, 1, VALIDATE_READ)) return -EINVAL;

  // printf("setting sigret for pid %d to %p\n", curproc->pid, sigret);

  curproc->sig.ret = (off_t)sigret;

  return 0;
}

int sys::sigaction(int sig, struct sigaction *act, struct sigaction *old) {
  if (!VALIDATE_RD(act, sizeof(*act))) return -EINVAL;
  if (!VALIDATE_WR(old, sizeof(*old))) return -EINVAL;


  if (sig < 0 || sig >= 64) return -EINVAL;
  *old = curproc->sig.handlers[sig];
  curproc->sig.handlers[sig] = *act;

  return 0;
}


int sys::sigreturn(void *ucontext) {
  assert(curthd->sig.handling != -1);
  curthd->sig.handling = -1;
  arch_sigreturn(ucontext);
  return 0;
}
int sys::sigprocmask(int how, unsigned long set, unsigned long *old_set) {
  if (old_set) {
    if (!curproc->mm->validate_pointer(old_set, sizeof(*old_set), VALIDATE_WRITE)) return -EFAULT;

    *old_set = curthd->sig.mask;
  }
  switch (how) {
    case SIG_BLOCK:
      curthd->sig.mask &= ~set;
      break;
    case SIG_UNBLOCK:
      curthd->sig.mask |= set;
      break;
    case SIG_SETMASK:
      curthd->sig.mask = set;
      break;
    default:
      return -EINVAL;
  }
  return 0;
}
