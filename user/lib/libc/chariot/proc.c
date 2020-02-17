#include <chariot.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

int spawn() { return syscall(SYS_spawn); }

int despawn(int p) { return syscall(SYS_despawn, p); }

int pctl(int pid, int request, ...) {
  void *arg;
  va_list ap;
  va_start(ap, request);
  arg = va_arg(ap, void *);
  va_end(ap);
  return syscall(SYS_pctl, pid, request, arg);
}

int startpidve(int pid, char *const path, char *const argv[],
               char *const envp[]) {
  /*
  printf("path=%p\n", path);
  printf("argv=%p\n", argv);
  printf("envp=%p\n", envp);
  */
  return syscall(SYS_startpidve, pid, path, argv, envp);
}

int startpidvpe(int pid, char *const file, char *const argv[],
                char *const envp[]) {
  char *path = NULL;  // TODO: getenv

  if (strchr(file, '/')) {
    return startpidve(pid, file, argv, envp);
  }

  // default path if there isn't one
  if (!path) path = "/usr/local/bin:/bin:/usr/bin";

  int k = strlen(file);  // TODO: strnlen
  if (k > 255) {
    // TODO: errno
    // errno = ENAMETOOLONG;
    return -1;
  }

  int l = strlen(path) + 1;
  char *z;

  for (char *p = path;; p = z) {
    char b[l + k + 1];
    z = strchr(p, ':');
    if (!z) z = p + strlen(p);
    if (z - p >= l) {
      if (!*z++) break;
      continue;
    }
    memcpy(b, p, z - p);
    b[z - p] = '/';
    memcpy(b + (z - p) + (z > p), file, k + 1);

    // TODO: validate that we can exec this
    int res = startpidve(pid, b, argv, envp);
    if (res == 0) {
      return 0;
    }

    // TODO: errno
    /*
                switch (errno) {
                case EACCES:
                        seen_eacces = 1;
                case ENOENT:
                case ENOTDIR:
                        break;
                default:
                        return -1;
                }
    */
    if (!*z++) break;
  }

  // TODO: errno
  // if (seen_eacces) errno = EACCES;
  return -1;
}

pid_t getpid(void) { return syscall(SYS_getpid); }

pid_t gettid(void) { return syscall(SYS_gettid); }
