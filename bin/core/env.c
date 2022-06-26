#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>

static void usage(void) {
  extern const char *__progname;
  (void)fprintf(stderr,
      "usage: %s [-i] [name=value ...] "
      "[command [argument ...]]\n",
      __progname);
  exit(1);
}

int main(int argc, char **argv) {
  extern char **environ;
  extern int optind;
  char *p;
  int ch;

#if 0
  while ((ch = getopt(argc, argv, "i-")) != -1) {
    switch (ch) {
      case '-': /* obsolete */
      case 'i':
        if ((environ = calloc(1, sizeof(char *))) == NULL) err(126, "calloc");
        break;
      default:
        usage();
    }
  }
#endif


  argc -= optind;
  argv += optind;

  for (; *argv && (p = strchr(*argv, '=')); ++argv) {
    *p++ = '\0';
    if (setenv(*argv, p, 1) == -1) {
      /* reuse 126, it matches the problem most */
      err(126, "setenv");
    }
  }

  if (*argv) {
    /*
     * return 127 if the command to be run could not be
     * found; 126 if the command was found but could
     * not be invoked
     */
    execvpe(argv[0], (const char **)argv, (const char **)environ);
    err((errno == ENOENT) ? 127 : 126, "%s", *argv);
  } else {
    for (int i = 0; environ[i]; i++) {
      printf("%s\n", environ[i]);
    }
  }


  return 0;
}
