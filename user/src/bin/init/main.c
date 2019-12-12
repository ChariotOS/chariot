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
      printf("%02X ", line[c]);
    }
    printf(" |");
    for (int c = 0; c < w; c++)
      printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
    printf("|\n");
  }
}

int main(int argc, char **argv) {
  /*
  for (int i = 0; i < 5; i++) spawn_proc("/bin/test");
  */

  int cons = open("/dev/urandom", O_RDWR);

  printf("fd=%d\n", cons);

  int len = 512;
  char *buf = malloc(len);

  read(cons, buf, len);
  hexdump(buf, len);

  free(buf);

  while (1) {
    yield();
  }
}

