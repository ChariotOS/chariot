#define _CHARIOT_SRC
#include <chariot/elf/exec_elf.h>
#include <chariot/fs/magicfd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

int __argc;
char **__argv;

// the normal libc environ variable
char **environ;
// user provided main (linked when library is used)
extern int main(int argc, char **argv, char **envp);
extern void stdio_init(void);
// in signal.c
extern void __signal_return_callback(void);
static void handle_executable(int fd);


extern void _init(void);
extern void _fini(void);

void libc_start(int argc, char **argv, char **envp) {

  __argc = argc;
  __argv = argv;
  environ = envp;

  // initialize signals
  sysbind_signal_init(__signal_return_callback);

  // initialize stdio seperate from global constructors
  // (chicken/egg problem)
  stdio_init();
  srand(time(NULL));

	_init();
	atexit(_fini);

	/*
  extern void (*__init_array_start[])(int, char **, char **) __attribute__((visibility("hidden")));
  extern void (*__init_array_end[])(int, char **, char **) __attribute__((visibility("hidden")));
  const size_t size = __init_array_end - __init_array_start;
  for (size_t i = 0; i < size; i++) {
		(*__init_array_start[i])(argc, argv, environ);
	}
	*/

  // TODO: parse envp and store in a better format!
  int code = main(__argc, __argv, environ);

  exit(code);

  printf("failed to exit!\n");

  // TODO: exit!
  while (1) {
  }
}
