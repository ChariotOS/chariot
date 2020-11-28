#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#define BSIZE 4096

int main(int argc, char **argv) {

  if (argc != 2) {
    printf("usage: %s <filename>\n", argv[0]);
    return -1;
  }

  int fd = open(argv[1], O_CREAT | O_WRONLY, 0666);
  if (fd < 0) {
    perror("touch: failed to create file");
    return 1;
  }

  return 0;
}
