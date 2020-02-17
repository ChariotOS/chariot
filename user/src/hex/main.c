#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

void hexdump(off_t off, void *vbuf, long len) {
  unsigned char *buf = vbuf;

  int w = 16;
  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;
    printf("%.8llx: ", (off + i));
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
        printf("   ");
      } else {
        printf("%02x ", line[c]);
      }
    }
    printf(" |");
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
      } else {
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    printf("|\n");
  }
}

#define BSIZE 4096
char buf[BSIZE];

int do_hexdump(int fd) {
  size_t off = 0;
  while (1) {
    long n = read(fd, buf, BSIZE);
    if (n < 0) {
      return -1;
    }
    if (n == 0) break;
    hexdump(off, buf, n);
    off += n;
  }

  return 0;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("usage: hex <files...>\n");
    return -1;
  }

  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    if (fd != -1) {
      do_hexdump(fd);
      close(fd);
    }
  }

  return 0;
}
