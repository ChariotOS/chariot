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
    // printf("%p ", line);

    for (int c = 0; c < w; c++) {
      if (i + c > len) {
        printf("\n");
        return;
      }
      printf("%02X ", line[c]);
    }
    printf(" |");
    for (int c = 0; c < w; c++)
      printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
    printf("|\n");
  }
}

char *gets(int fd, char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(fd, &c, 1);
    if (cc < 1) break;


    if (c == 0x7F) {
      if (i != 0) {
        buf[--i] = 0;
      }
    } else {
      buf[i++] = c;
    }
    if (c == '\n' || c == '\r') break;
  }
  buf[i] = '\0';
  return buf;
}



int main(int argc, char **argv) {
  /*
  for (int i = 0; i < 5; i++) spawn_proc("/bin/test");
  */

  int cons = open("/dev/console", O_RDWR);

  char buf[100];

  while (1) {
    printf("$ ");
    gets(cons, buf, 100);


    buf[strlen(buf)-1] = 0;
    printf("you typed: '%s'\n", buf);
  }

  while (1) {
    yield();
  }
}

