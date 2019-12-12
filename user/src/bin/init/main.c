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
  for (int i = 0; i < len; i++) {
    printf("%02x ", buf[i]);
  }

  printf(" ");

  for (int i = 0; i < len; i++) {
    char c = buf[i];
    printf("%c", (c < 0x20) || (c > 0x7e) ? '.' : c);
  }
  printf("\n");
}



int main(int argc, char **argv) {
  /*
  for (int i = 0; i < 5; i++) {
    spawn_proc("/bin/test");
  }
  */

  // pctl(PID_SELF, PCTL_EXIT);

  int i = 0;

  int cases[255];
  for (int i = 0; i < 255; i++) cases[i] = 0;

  float avg = 0;

  while (1) {
    yield();
  }

  int fd = open("/dev/urandom", O_RDONLY);

#define RDSZ 255
  unsigned char buf[RDSZ];

  int n = 0;
  while (1) {
    read(fd, buf, RDSZ);
    for (int i = 0; i < RDSZ; i++) {
      cases[(unsigned int)buf[i]]++;
    }
    avg = 0;
    long total = 0;
    for (int i = 0; i < 255; i++) {
      total += cases[i];
      avg += cases[i] * i;
    }
    avg /= total;
    n++;
    if (n % 1000 == 0) printf("%d: %d\n", n, (int)avg);
  }

  while (1) {
    continue;
    int fd = open("/etc/passwd", O_RDONLY);
    printf("%-4d: ", i++);

    char buf[500];
    int n = read(fd, buf, 255);

    // write to stdout
    write(1, buf, n);
    syscall(SYS_close, fd);
  }
}

