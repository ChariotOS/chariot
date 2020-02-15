#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void hexdump(void *vbuf, long len) {
  unsigned char *buf = vbuf;

  int w = 16;
  for (int i = 0; i < len; i += w) {

    unsigned char *line = buf + i;
    printf("%p: ", line);
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


int getc(void) {
  int c = 0;
  if (read(0, &c, 1) != 1) {
    return -1;
  }
  return c;
}

char *read_line(int fd, char *prompt, int *len_out) {
  int i = 0;
  long max = 16;

  char *buf = malloc(max);
  memset(buf, 0, max);

  printf("%s", prompt);
  fflush(stdout);

  for (;;) {
    if (i + 1 >= max) {
      max *= 2;
      buf = realloc(buf, max);
    }

    int c = getc();

    if (c == -1) break;
    if (c == '\n' || c == '\r') break;

    switch (c) {
      case 0x7F:
        if (i != 0) {
          buf[--i] = 0;
        } else {
          printf("\r%s", prompt);
          fflush(stdout);
        }
        break;

      default:
        buf[i++] = c;
        break;
    }
  }
  buf[i] = '\0';  // null terminate
  if (len_out != NULL) *len_out = i;
  return buf;
}
