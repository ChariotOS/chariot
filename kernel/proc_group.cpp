#include <sched.h>
#include <syscall.h>
#include <cpu.h>

int sys::setpgid(int pid, int pgid) {
  /* TODO: EACCESS (has execve'd) */
  if (pgid < 0) return -EINVAL;

  if (pgid == 0) pgid = curproc->pid;

  Process *proc = NULL;
  if (pid == 0) {
    pid = curproc->pid;
    proc = curproc;  // optimization
  } else {
    // if there is a process `pid`, the main thread must exist
    // with the same `tid`.
    auto thd = Thread::lookup(pid);
    if (thd == nullptr) return -ESRCH;
    // TODO: permissions?
    proc = &thd->proc;
  }
  if (proc == NULL) return -ESRCH;

  /* TODO: EPERM (move proc to another session (?)) */



  proc->datalock.lock();
  proc->pgid = pgid;
  proc->datalock.unlock();

  return 0;
}

int sys::getpgid(int pid) {
  Process *proc = NULL;
  if (pid == 0) {
    pid = curproc->pid;
    proc = curproc;  // optimization
  } else {
    // if tphere is a process `pid`, the main thread must exist
    // with the same `tid`.
    auto thd = Thread::lookup(pid);
    if (thd == nullptr) return -ESRCH;
    // TODO: permissions?
    proc = &thd->proc;
  }
  if (proc == NULL) return -ESRCH;

  /* TODO: probably lock the process data? */
  return proc->pgid;
}
