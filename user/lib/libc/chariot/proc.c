#include <chariot.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>

int spawn() {
  return syscall(SYS_spawn);
  return pctl(0, PCTL_SPAWN);
}

static int nt_array_len(char *const *a) {
  if (a == NULL) return 0;
  int i = 0;
  for (i = 0; a[i] != NULL; i++)
    ;
  return i;
}


int pctl(int pid, int request, ...) {
  void *arg;
  va_list ap;
  va_start(ap, request);
  arg = va_arg(ap, void *);
  va_end(ap);
  return syscall(SYS_pctl, pid, request, arg);
}


int startpidve(int pid, char *const path, char *const argv[], char *const envp[]) {
  printf("path=%p\n", path);
  printf("argv=%p\n", argv);
  printf("envp=%p\n", envp);
  return syscall(SYS_startpidve, pid, path, argv, envp);
}


pid_t getpid(void) {
  return syscall(SYS_getpid);
}

pid_t gettid(void) {
  return syscall(SYS_gettid);
}
