
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
  while (1) {
  }
}
