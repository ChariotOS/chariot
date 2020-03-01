#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int __argc;
char **__argv;

// the normal libc environ variable
char **environ;

// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);

extern void stdio_init(void);

void libc_start(int argc, char **argv, char **envp) {
  __argc = argc;
  __argv = argv;
  environ = envp;
  // initialize stdio
  stdio_init();

  // TODO: parse envp and store in a better format!
  exit(main(__argc, __argv, environ));
  printf("failed to exit!\n");
  // TODO: exit!
  while (1) {
  }
}
