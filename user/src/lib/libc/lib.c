#include <stdarg.h>
#include <stdio.h>

// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);


void libc_start() {
  main(0, 0, 0);
  // TODO: exit!
  while (1) {
  }
}
