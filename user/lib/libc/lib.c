#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int __argc;
char **__argv;
char **__envp;

// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);

extern void stdio_init(void);

void libc_start(int argc, char **argv, char **envp) {
  // initialize stdio
  stdio_init();

  // TODO: parse envp and store in a better format!
  int res = main(argc, argv, envp);
  printf("back from main!\n");

  syscall(SYS_exit_proc, res);
  // TODO: exit!
  while (1) {
  }
}
