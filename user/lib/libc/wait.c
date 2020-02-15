#include <sys/wait.h>
#include <sys/syscall.h>
#include <stdlib.h>

pid_t wait(int *status) {
	return waitpid(-1, status, 0);
}


pid_t waitpid(pid_t p, int *status, int flags) {
  int dummy_stat;
  if (status == NULL) status = &dummy_stat;
  return syscall(SYS_waitpid, p, status, flags);
}
