#include <chariot.h>
#include <sys/syscall.h>


int spawn() {
  return syscall(SYS_spawn);
}


int cmdpidve(int pid, char *const path, char * const argv[], char *const envp[]) {
  return syscall(SYS_cmdpidve, pid, path, argv, envp);
}

int cmdpidv(int pid, char *const path, char * const argv[]) {
  // TODO: get envp
  char *env[] = {0};
  return cmdpidve(pid, path, argv, env);
}
