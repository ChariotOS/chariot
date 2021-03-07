#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BSIZE 4096

int main(int argc, char **argv) {
  if (argc == 1) {
    while (1) {
      int c = fgetc(stdin);
      if (c == EOF) return 0;
      putc(c, stdout);
    }
  }

  char *buf = malloc(BSIZE);
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);

    while (fd != -1) {
      long n = read(fd, buf, BSIZE);
      if (n < 0) {
        printf("failed to read '%s'\n", argv[i]);
        break;
      }
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
