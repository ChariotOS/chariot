#include <chariot.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <unistd.h>


typedef int (*exec_fn_t)(const char *, const char **, const char **);


int spawn() { return sysbind_spawn(); }

int despawn(int p) { return sysbind_despawn(p); }

int pctl(int pid, int request, ...) {
  void *arg;
  va_list ap;
  va_start(ap, request);
  arg = va_arg(ap, void *);
  va_end(ap);
  return errno_wrap(sysbind_pctl(pid, request, (unsigned long)arg));
}

int startpidve(int pid, char *const path, char *const argv[],
               char *const envp[]) {
  return errno_wrap(
      sysbind_startpidve(pid, path, (const char **)argv, (const char **)envp));
}

int startpidvpe(int pid, char *const file, char *const argv[],
                char *const envp[]) {
  int seen_eacces = 0;

  if (strchr(file, '/')) {
    return startpidve(pid, file, argv, envp);
  }

  char *path = getenv("PATH");  // TODO: getenv

  // default path if there isn't one
  if (!path) path = "/usr/local/bin:/bin:/usr/bin";

  int k = strlen(file);  // TODO: strnlen
  if (k > 255) {
    // TODO: errno
    errno = ENAMETOOLONG;
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

    switch (errno) {
      case EACCES:
        seen_eacces = 1;
      case ENOENT:
      case ENOTDIR:
        break;
      default:  // TODO
        break;
        return -1;
    }
    if (!*z++) {
      break;
    }
  }

  // TODO: errno
  if (seen_eacces) errno = EACCES;
  return -1;
}

extern const char **environ;

int execve(const char *path, char *const argv[], char *const envp[]) {
  return startpidve(-1, (char *)path, argv, envp);
}

int execvp(const char *file, char *const argv[]) {
  return startpidve(-1, (char *const)file, (char *const *)argv,
                    (char *const *)environ);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
  return startpidvpe(-1, (char *)file, argv, envp);
}


int execl(const char *path, const char *arg0, ... /*, (char *)0 */) {
  int argc;
  va_list ap;
  va_start(ap, arg0);
  for (argc = 1; va_arg(ap, const char *); argc++)
    ;
  va_end(ap);
  {
    int i;
    char *argv[argc + 1];
    va_start(ap, arg0);
    argv[0] = (char *)arg0;
    for (i = 1; i < argc; i++) argv[i] = va_arg(ap, char *);
    argv[i] = NULL;
    va_end(ap);
    return execv(path, argv);
  }
}


int execle(const char *path, const char *argv0, ...) {
  int argc;
  va_list ap;
  va_start(ap, argv0);
  for (argc = 1; va_arg(ap, const char *); argc++)
    ;
  va_end(ap);
  {
    int i;
    char *argv[argc + 1];
    char **envp;
    va_start(ap, argv0);
    argv[0] = (char *)argv0;
    for (i = 1; i <= argc; i++) argv[i] = va_arg(ap, char *);
    envp = va_arg(ap, char **);
    va_end(ap);
    return execve(path, argv, envp);
  }
}

int execlp(const char *file, const char *argv0, ...) {
  int argc;
  va_list ap;
  va_start(ap, argv0);
  for (argc = 1; va_arg(ap, const char *); argc++)
    ;
  va_end(ap);
  {
    int i;
    char *argv[argc + 1];
    va_start(ap, argv0);
    argv[0] = (char *)argv0;
    for (i = 1; i < argc; i++) argv[i] = va_arg(ap, char *);
    argv[i] = NULL;
    va_end(ap);
    return execvp(file, argv);
  }
}


int execv(const char *path, char *const argv[]) {
  return execve(path, argv, (char *const *)environ);
}


pid_t getpid(void) { return sysbind_getpid(); }

pid_t gettid(void) { return sysbind_gettid(); }
