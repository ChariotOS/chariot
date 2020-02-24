#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char **argv, char **envp) {
  for (int i = 0; envp[i] != NULL; i++) {
    printf("%s\n", envp[i]);
  }

  return 0;
}
