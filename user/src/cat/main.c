#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BSIZE 4096

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("usage: cat <files...>\n");
    return -1;
  }

  char *buf = malloc(BSIZE);
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);

    while (fd != -1) {
      size_t n = read(fd, buf, BSIZE);
      write(1, buf, n);

      if (n < BSIZE) {
        break;
      }
    }

    close(fd);


  }

  free(buf);

  return 0;
}
