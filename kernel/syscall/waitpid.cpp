#include <process.h>
#include <wait_flags.h>
#include <cpu.h>

long sys::waitpid(long pid, int *status_ptr, int flags) {

  if (status_ptr != NULL && !curproc->addr_space->validate_pointer(status_ptr, sizeof(*status_ptr), VPROT_WRITE)) {
    return -1;
  }

  // local status which will be built up as we figure out what to wait on.
  int status = 0;

  if (status_ptr != NULL) *status_ptr = status;
  return -1;
}
