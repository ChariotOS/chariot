#include <chariot.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


#define MAX_ARGS 255
char *read_line(int fd, char *prompt, int *len_out);
int parseline(const char *, char **argv);



int main(int argc, char **argv, char **envp) {

  int arg_buflen = sizeof(char *) * MAX_ARGS;
  char **args = malloc(arg_buflen);

  while (1) {
    int len = 0;

    memset(args, 0, arg_buflen * sizeof(char *));

    char *buf = read_line(0, "# ", &len);

    len = strlen(buf);
    if (len == 0) goto cleanup;

    if (strcmp(buf, "exit") == 0) exit(0);

    parseline(buf, args);

    pid_t pid = spawn();
    if (pid <= -1) {
      printf("Error spawning, code=%d\n", pid);
      goto cleanup;
    }

    int start_res = startpidve(pid, args[0], args, envp);
    // printf("sr:%d\n", start_res);
    if (start_res == 0) {
      int stat = 0;
      waitpid(pid, &stat, 0);
      // printf("r:%x s:%x\n", r, stat);
    }

  cleanup:
    free(buf);
  }

  free(args);

  return 0;
}

int getc(void) {
  int c = 0;
  if (read(0, &c, 1) != 1) {
    return -1;
  }
  return c;
}

char *read_line(int fd, char *prompt, int *len_out) {
  int i = 0;
  long max = 16;

  char *buf = malloc(max);
  memset(buf, 0, max);

  printf("%s", prompt);
  fflush(stdout);

  for (;;) {
    if (i + 1 >= max) {
      max *= 2;
      buf = realloc(buf, max);
    }

    int c = getc();

    if (c == -1) break;
    if (c == '\n' || c == '\r') break;

    switch (c) {
      case 0x7F:
        if (i != 0) {
          buf[--i] = 0;
        } else {
          printf("\r%s", prompt);
          fflush(stdout);
        }
        break;

      default:
        buf[i++] = c;
        break;
    }
  }
  buf[i] = '\0';  // null terminate
  if (len_out != NULL) *len_out = i;
  return buf;
}


int parseline(const char *cmdline, char **argv) {
  char *buf = malloc(strlen(cmdline) + 1); /* ptr that traverses command line */
  char *delim;                             /* points to first space delimiter */
  int argc;                                /* number of args */
  int bg;                                  /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf)] = ' ';       /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0) {
    argv[--argc] = NULL;
  }
  return bg;
}
