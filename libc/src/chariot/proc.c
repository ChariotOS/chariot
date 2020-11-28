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


extern const char **environ;

int execve(const char *path, const char *argv[], const char *envp[]) {
  return sysbind_execve((char *)path, (const char **)argv, (const char **)envp);
}

int execvp(const char *file, const char *argv[]) { return execvpe(file, argv, environ); }

#define NAME_MAX 255
#define PATH_MAX 4096

int execvpe(const char *file, const char *argv[], const char *envp[]) {
  const char *p, *z, *path = getenv("PATH");
  size_t l, k;
  int seen_eacces = 0;

  errno = ENOENT;
  if (!*file) return -1;

  if (strchr(file, '/')) return execve(file, argv, envp);

  if (!path) path = "/usr/local/bin:/bin:/usr/bin";
  k = strnlen(file, NAME_MAX + 1);
  if (k > NAME_MAX) {
    errno = ENAMETOOLONG;
    return -1;
  }
  l = strnlen(path, PATH_MAX - 1) + 1;

  for (p = path;; p = z) {
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
    execve(b, argv, envp);
    switch (errno) {
      case EACCES:
        seen_eacces = 1;
      case ENOENT:
      case ENOTDIR:
        break;
      default:
        return -1;
    }
    if (!*z++) break;
  }
  if (seen_eacces) errno = EACCES;
  return -1;
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
    const char *argv[argc + 1];
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
    const char *argv[argc + 1];
    va_start(ap, argv0);
    argv[0] = (char *)argv0;
    for (i = 1; i <= argc; i++) argv[i] = va_arg(ap, char *);
    const char **envp = va_arg(ap, const char **);
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
    const char *argv[argc + 1];
    va_start(ap, argv0);
    argv[0] = (char *)argv0;
    for (i = 1; i < argc; i++) argv[i] = va_arg(ap, char *);
    argv[i] = NULL;
    va_end(ap);
    return execvp(file, argv);
  }
}


int execv(const char *path, const char *argv[]) { return execve(path, argv, (const char **)environ); }


pid_t getpid(void) { return sysbind_getpid(); }

pid_t gettid(void) { return sysbind_gettid(); }
