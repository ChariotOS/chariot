#include <cpu.h>
#include <syscall.h>
#include <wait_flags.h>

long sys::waitpid(int pid, int *status_ptr, int flags) {
  if (status_ptr != NULL &&
      !curproc->mm->validate_pointer(status_ptr, sizeof(*status_ptr), PROT_WRITE)) {
    return -1;
  }

  // local status which will be built up as we figure out what to wait on.
  int status = 0;

  int res = sched::proc::do_waitpid(pid, status, flags);

  if (status_ptr != NULL) *status_ptr = status;
  return res;
}
