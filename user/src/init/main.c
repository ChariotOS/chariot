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


static volatile long counter = 0;

void *thread_func(void *p) {
  int num = (int)p;
  printf("hello!     p=%d\n", num);
  while (1) {
    counter++;
    yield();
  }
  syscall(SYS_exit_task, 0);
  return NULL;
}


int main(int argc, char **argv) {
  // spawn_proc("/bin/vidtest");

  for (int i = 0; i < 5; i++) {
    pthread_t thd;
    pthread_create(&thd, NULL, thread_func, (void*)(long)i);
    printf("spawned %d\n", i);
  }

  while (0) {
    yield();
  }

  // open the console (till we get stdin/out/err opening by default)
  while (1) {
    int len = 0;


    char prompt[40];

    snprintf(prompt, 40, "%d> ", counter);


    char *buf = read_line(0, prompt, &len);

    len = strlen(buf);
    if (len == 0) goto noprint;

    printf("len=%d\n", len);
    printf("buf=%p\n", buf);
    printf("you typed: '%s'\n", buf);
    printf("counter=%ld\n", counter);
    hexdump(buf, len);

  noprint:
    free(buf);
  }

  while (1) {
    yield();
  }
}

