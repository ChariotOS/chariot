#include <chariot.h>
#include <fcntl.h>
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

void hexdump(void *vbuf, int len) {
  unsigned char *buf = vbuf;

  int w = 16;
  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;
    for (int c = 0; c < w; c++) {
      if (i + c > len) {
        printf("   ");
      } else {
        printf("%02X ", line[c]);
      }
    }
    printf(" |");
    for (int c = 0; c < w; c++) {
      if (i + c > len) {
      } else {
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    printf("|\n");
  }
}

char *read_line(int fd, char *prompt, int *len_out) {
  int i = 0;
  int cc;
  char c;
  int max = 16;

  char *buf = malloc(max);
  memset(buf, 0, max);

  printf("%s", prompt);

  for (;;) {
    if (i + 1 >= max) {
      max *= 2;
      buf = realloc(buf, max);
    }

    cc = read(fd, &c, 1);
    if (cc < 1) break;

    if (c == 0x7F) {
      if (i != 0) {
        buf[--i] = 0;
      } else {
        for (int j = 0; j < strlen(prompt); j++) {
          printf("\b \b");
        }
        printf("%s", prompt);
      }
    } else {
      buf[i++] = c;
    }
    if (c == '\n' || c == '\r') break;
  }
  buf[--i] = '\0';
  if (len_out != NULL) *len_out = i;
  return buf;
}

int main(int argc, char **argv) {
  // for (int i = 0; i < 6; i++) spawn_proc("/bin/test");
  int cons = open("/dev/console", O_RDWR);

  while (1) {
    int len = 0;
    char *buf = read_line(cons, "init> ", &len);

    printf("buf=%p\n", buf);
    printf("you typed: '%s'\n", buf);
    hexdump(buf, len);

    free(buf);
  }

  while (1) {
    yield();
  }
}

