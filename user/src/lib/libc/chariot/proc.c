#include <chariot.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/syscall.h>

int spawn() { return pctl(0, PCTL_SPAWN); }

int cmdpidve(int pid, char *const path, char *const argv[],
             char *const envp[]) {
  struct pctl_cmd_args args;

  if (path == NULL) return -1;

  args.path = path;

  if (argv == NULL) return -1;

  for (args.argc = 0; argv[args.envc] != NULL; args.envc++) {
  }
  args.argv = (char **)argv;

  if (envp != NULL) {
    for (args.envc = 0; envp[args.envc] != NULL; args.envc++) {
    }
  }
  args.envv = (char **)envp;

  return pctl(pid, PCTL_CMD, &args);
}

int cmdpidv(int pid, char *const path, char *const argv[]) {
  // TODO: get envp
  char *env[] = {0};
  return cmdpidve(pid, path, argv, env);
}

int pctl(int pid, int request, ...) {
  void *arg;
  va_list ap;
  va_start(ap, request);
  arg = va_arg(ap, void *);
  va_end(ap);
  return syscall(SYS_pctl, pid, request, arg);
}
