#include <chariot.h>
#include <chariot/pctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

char *read_line(int fd, char *prompt, int *len_out);
void hexdump(void *vbuf, long len);

int spawn_proc(char *bin) {
  int pid = spawn();

  if (pid == -1) {
    return -1;
  }
  char *cmd[] = {bin, 0};
  int res = cmdpidv(pid, cmd[0], cmd);
  if (res != 0) {
    return -1;
  }

  return pid;
}

#define MAX_ARGS 255

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

uint32_t x;  // The state can be seeded with any value.
// Call next() to get 32 pseudo-random bits, call it again to get more bits.
// It may help to make this inline, but you should see if it benefits your
// code.
uint32_t next(void) {
  uint32_t z = (x += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

long val = 0;

int main(int argc, char **argv) {
  int arg_buflen = sizeof(char *) * MAX_ARGS;
  char **args = malloc(arg_buflen);

  pid_t pid = spawn_proc("/bin/test/exit");

  int res = waitpid(pid, NULL, 0);
  printf("res=%d\n", res);

  while (1) {
    int len = 0;

    memset(args, 0, arg_buflen * sizeof(char *));

    char *buf = read_line(0, "# ", &len);

    len = strlen(buf);
    if (len == 0) goto noprint;

    hexdump(buf, len);

    parseline(buf, args);

    pid_t pid = spawn_proc(args[0]);

    if (pid != -1) {
      int res = waitpid(pid, NULL, 0);
      printf("res=%d\n", res);
    }

    /*
    if (bg) printf("Background Process\n");
    for (int i = 0; i < MAX_ARGS; i++) {
      if (args[i] == NULL) break;
      printf("%d:'%s'\n", i, args[i]);
    }
    */

  noprint:
    free(buf);
  }

  free(args);

  while (1) {
    yield();
  }
}

