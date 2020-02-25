#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int errno;

int *__errno_location(void) {
  return &errno;
}

ssize_t write(int fd, const void *buf, size_t count) {
  // just forward to the system call
  return errno_syscall(SYS_write, fd, buf, count);
}

ssize_t read(int fd, void *buf, size_t count) {
  // just forward to the system call
  return errno_syscall(SYS_read, fd, buf, count);
}

off_t lseek(int fd, off_t offset, int whence) {
  return errno_syscall(SYS_lseek, fd, offset, whence);
}

int close(int fd) { return errno_syscall(SYS_close, fd); }

int chdir(const char *path) {
  return errno_syscall(SYS_chdir, path);
}

int opterr = 1, /* if error message should be printed */
    optind = 1, /* index into parent argv vector */
    optopt,     /* character checked for validity */
    optreset;   /* reset getopt */
char *optarg;   /* argument associated with option */

#define BADCH (int)'?'
#define BADARG (int)':'
#define EMSG ""

/*
 * getopt --
 *      Parse argc/argv argument vector.
 */
int getopt(int nargc, char *const nargv[], const char *ostr) {
  static char *place = EMSG; /* option letter processing */
  const char *oli;           /* option letter list index */

  if (optreset || !*place) { /* update scanning pointer */
    optreset = 0;
    if (optind >= nargc || *(place = nargv[optind]) != '-') {
      place = EMSG;
      return (-1);
    }
    if (place[1] && *++place == '-') { /* found "--" */
      ++optind;
      place = EMSG;
      return (-1);
    }
  } /* option letter okay? */
  if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr, optopt))) {
    /*
     * if the user didn't specify '-' as an option,
     * assume it means -1.
     */
    if (optopt == (int)'-') return (-1);
    if (!*place) ++optind;
    if (opterr && *ostr != ':') (void)printf("illegal option -- %c\n", optopt);
    return (BADCH);
  }
  if (*++oli != ':') { /* don't need argument */
    optarg = NULL;
    if (!*place) ++optind;
  } else {      /* need an argument */
    if (*place) /* no white space */
      optarg = place;
    else if (nargc <= ++optind) { /* no arg */
      place = EMSG;
      if (*ostr == ':') return (BADARG);
      if (opterr) (void)printf("option requires an argument -- %c\n", optopt);
      return (BADCH);
    } else /* white space */
      optarg = nargv[optind];
    place = EMSG;
    ++optind;
  }
  return (optopt); /* dump back option letter */
}

uid_t getuid(void) { return syscall(SYS_getuid); }
uid_t geteuid(void) { return syscall(SYS_geteuid); }
gid_t getgid(void) { return syscall(SYS_getgid); }
gid_t getegid(void) { return syscall(SYS_getegid); }

int access(const char *path, int amode) {
  // TODO: access syscall
  return 0;
}


