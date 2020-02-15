#include <chariot.h>
#include <chariot/pctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

char *read_line(int fd, char *prompt, int *len_out);
void hexdump(void *vbuf, long len);

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
  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++) close(i + 1);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);

  printf("[init] hello, friend\n");

  printf("tid=%d, pid=%d\n", gettid(), getpid());

  int arg_buflen = sizeof(char *) * MAX_ARGS;
  char **args = malloc(arg_buflen);

  int size = 8 * 4096;
  int *reg =
      mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON, -1, 0);
  mrename(reg, "my region");
  memset(reg, 0, size / 2);
  munmap(reg, size);

  char *envs[] = {NULL};
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

    int start_res = startpidve(pid, args[0], args, envs);
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

  while (1) {
    yield();
  }
}

