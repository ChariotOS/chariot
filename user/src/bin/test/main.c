#include <chariot.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv, char **envp) {
  printf("test: argc = %d, argv = %p\n", argc, argv);
  for (int i = 0; i < argc; i++) {
    printf("  argv[%d] = %p '%s'\n", i, argv[i], argv[i]);
  }

  while (1) {
    // printf("B");
  }
}
