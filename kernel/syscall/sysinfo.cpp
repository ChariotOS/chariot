#include <sched.h>
#include <syscall.h>
#include <types.h>
#include <errno.h>
#include <cpu.h>


int sys::sysinfo(struct sysinfo *u_info) {
  if (!VALIDATE_WR(u_info, sizeof(*u_info))) {
    return -EINVAL;
  }

	// struct sysinfo i;


  return 0;
}
