#include <chariot.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/syscall.h>

int spawn() { return pctl(0, PCTL_SPAWN); }

static int nt_array_len(char *const *a) {
  if (a == NULL) return 0;
  int i = 0;
  for (i = 0; a[i] != NULL; i++)
    ;
  return i;
}

int cmdpidve(int pid, char *const path, char *const argv[],
             char *const envp[]) {
  // some quick safety checks
  if (pid == -1) return -1;
  if (path == NULL) return -1;
  if (argv == NULL) return -1;

  struct pctl_cmd_args args;
  args.path = path;

  args.argc = nt_array_len(argv);
  args.argv = (char **)argv;

  args.envc = nt_array_len(envp);
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
