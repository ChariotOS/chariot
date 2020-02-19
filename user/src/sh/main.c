#include <chariot.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"
#define C_RESET "\x1b[0m"
#define C_GRAY "\x1b[90m"

#define MAX_ARGS 255
char *read_line(int fd, char *prompt, int *len_out);
int parseline(const char *, char **argv);

int main(int argc, char **argv, char **envp) {
  int arg_buflen = sizeof(char *) * MAX_ARGS;
  char **args = malloc(arg_buflen);

  char prompt[64];

  snprintf(prompt, 64, C_GREEN "sh# " C_RESET, argv[0]);

  while (1) {
    int len = 0;

    memset(args, 0, arg_buflen * sizeof(char *));

    char *buf = read_line(0, prompt, &len);

    len = strlen(buf);
    if (len == 0) goto cleanup;

    if (strcmp(buf, "exit") == 0) exit(0);

    int bg = parseline(buf, args);

    if (strcmp(args[0], "cd") == 0) {
      int res = chdir(args[1]);
      if (res != 0) {
        printf("cd: '%s' could not be entered\n", args[1]);
      }

      goto cleanup;
    }

    pid_t pid = spawn();
    if (pid <= -1) {
      printf("Error spawning, code=%d\n", pid);
      goto cleanup;
    }

    int start_res = startpidvpe(pid, args[0], args, envp);
    if (start_res == 0) {
      int stat = 0;
      if (!bg) {
        waitpid(pid, &stat, 0);
        // printf("status=%x\n", stat);
      }
    } else {
      printf("failed to execute: '%s'\n", buf);
      despawn(pid);
    }

  cleanup:
    free(buf);
  }

  free(args);

  return 0;
}


char *read_line(int fd, char *prompt, int *len_out) {
  int i = 0;
  long max = 512;

  char *buf = malloc(max);
  memset(buf, 0, max);

  printf("%s", prompt);
  fflush(stdout);

  for (;;) {
    if (i + 1 >= max-1) {
      max *= 2;
      buf = realloc(buf, max);
    }

    int c = fgetc(stdin);

    if (c == -1) break;
    if (c == '\n' || c == '\r') break;

    switch (c) {
      case 0x7F:
        if (i != 0) {
          buf[--i] = 0;
        }
        break;

      default:
        buf[i++] = c;
        break;
    }

    printf("\r%s%s", prompt, buf);
    fflush(stdout);
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
