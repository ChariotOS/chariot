#include <process.h>
#include <wait_flags.h>
#include <cpu.h>

long sys::waitpid(long pid, int *status_ptr, int flags) {
  printk("waiting on %ld\n", pid);

  if (status_ptr != NULL && !cpu::proc()->mm.validate_pointer(status_ptr, sizeof(*status_ptr), VPROT_WRITE)) {
    return -1;
  }

  // local status which will be built up as we figure out what to wait on.
  int status = 0;


  printk("waitpid(%u, 0x%p, 0x%08x\n", pid, status, flags);

  if (status_ptr != NULL) *status_ptr = status;
  return -1;
}
