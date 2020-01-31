#include <chariot.h>
#include <chariot/pctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

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

void hexdump(void *vbuf, long len) {
  unsigned char *buf = vbuf;

  int w = 16;
  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
        printf("   ");
      } else {
        printf("%02X ", line[c]);
      }
    }
    printf(" |");
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
      } else {
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    printf("|\n");
  }
}

int getc(void) {
  int c;
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
static void *worker(void *arg) {
  while (1) {
    val = next();
  }
  return NULL;
}

int main(int argc, char **argv) {
  int arg_buflen = sizeof(char *) * MAX_ARGS;
  char **args = malloc(arg_buflen);

#define N 64

  pthread_t thd;

  pthread_create(&thd, NULL, worker, NULL);

  while (1) {
    char *buf = malloc(N);
    for (int i = 0; i < N; i++) buf[i] = val;

    hexdump(buf, N);

    free(buf);
  }

  while (1) {
    int len = 0;

    memset(args, 0, arg_buflen);

    char *buf = read_line(0, "# ", &len);

    len = strlen(buf);
    if (len == 0) goto noprint;

    int fd = open(buf, O_RDONLY);

    if (fd != -1) {
      printf("found!\n");

      char buf[512];

      int n = read(fd, buf, 512);

      if (n == -1) {
        printf("can't read\n");
        goto done;
      }
      if (n == 0) {
        printf("done!\n");
        goto done;
      }
      if (n > 0) {
        hexdump(buf, n);
      }
    done:
      close(fd);
    }

    if (0) {
      hexdump(buf, len);

      int bg = parseline(buf, args);

      if (bg) printf("Background Process\n");
      for (int i = 0; i < MAX_ARGS; i++) {
        if (args[i] == NULL) break;
        printf("%d:'%s'\n", i, args[i]);
      }
    }

  noprint:
    free(buf);
  }

  free(args);

  while (1) {
    yield();
  }
}

