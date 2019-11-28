#include <stdarg.h>
#include <stdio.h>

// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);

extern void stdio_init(void);

void libc_start(int argc, char **argv, char **envp) {
  // initialize stdio
  stdio_init();

  // TODO: parse envp and store in a better format!
  main(argc, argv, envp);
  // TODO: exit!
  while (1) {
  }
}
