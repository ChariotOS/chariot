#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <chariot.h>



int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    printf("%s\n", argv[i]);
    if (i != argc - 1) printf(" ");
  }
  printf("\n");

  return 0;
}
