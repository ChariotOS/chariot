#include <unistd.h>


// TODO: FILE* interface
int putchar(int c) {
  // write one char
  write(0, &c, 1);
  return 0;
}
