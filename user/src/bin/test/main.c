
#include <chariot.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int fib(int i) {
  if (i < 2) return i;

  return fib(i - 1) + fib(i - 2);
}

int main(int argc, char **argv, char **envp) {
  printf("hello, world\n");

  int fd = 0;

  char buf[255];

  while (1) {

    for (int i = 0; i < 3; i++) {
      switch (i) {
        case 0:
          fd = open("/etc/passwd", O_RDONLY);
          break;
        case 1:
          read(fd, buf, 255);
          break;
        case 2:
          fib(50);
          syscall(SYS_close, fd);
          break;
      }
    }
  }
}
