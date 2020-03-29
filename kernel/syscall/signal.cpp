#include <cpu.h>
#include <sched.h>
#include <sig.h>
#include <syscall.h>

int sys::signal_init(void *sigret) {
  // validate that the trampoline is valid
  // (currently mapped, as to avoid segfaults)
  if (!curproc->mm->validate_pointer(sigret, 1, VALIDATE_READ)) return -EINVAL;

  // printk("setting sigret for pid %d to %p\n", curproc->pid, sigret);

  curproc->sig.ret = (off_t)sigret;

  return 0;
}

int sys::signal(int sig, void *handler) { return -ENOSYS; }
int sys::sigreturn(void) { return -ENOSYS; }
int sys::sigprocmask(int how, unsigned long set, unsigned long *old_set) {
  if (old_set) {
    if (!curproc->mm->validate_pointer(old_set, sizeof(*old_set),
				       VALIDATE_WRITE))
      return -EFAULT;

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

