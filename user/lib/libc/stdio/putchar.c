#include <unistd.h>
#include <stdio.h>

int putchar(int c) {
  // write one char
  fwrite(&c, 1, 1, stdout);
  return 0;
}
