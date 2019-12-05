#include <chariot.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>

int main(int argc, char **argv, char **envp) {
  printf("hello, world\n");
  while (1) {
    yield();
  }
}
