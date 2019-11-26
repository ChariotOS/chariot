#include <stdarg.h>
#include <stdio.h>

// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);


void libc_start(int argc, char **argv, char **envp) {
  // TODO: parse envp and store in a better format!
  main(argc, argv, envp);
  // TODO: exit!
  while (1) {
  }
}
