#include <sys/wait.h>
#include <sys/syscall.h>

pid_t wait(int *status) {
	return waitpid((pid_t)-1, status, 0);
}


pid_t waitpid(pid_t p, int *status, int flags) {
  int dummy_stat;
  if (status == 0) status = &dummy_stat;
  return syscall(SYS_waitpid, p, status, flags);
}
