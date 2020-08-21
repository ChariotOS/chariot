#define _CHARIOT_SRC
#include <chariot/elf/exec_elf.h>
#include <chariot/fs/magicfd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <time.h>
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
typedef void (*func_ptr)(void);
extern func_ptr __init_array_start[0], __init_array_end[0];
static void handle_executable(int fd);


static void call_global_constructors(void) {
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }
}



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

  call_global_constructors();


  // TODO: parse envp and store in a better format!
  exit(main(__argc, __argv, environ));
  printf("failed to exit!\n");
  // TODO: exit!
  while (1) {
  }

  handle_executable(MAGICFD_EXEC);
}


void handle_executable(int fd) {
  struct stat st;
  fstat(fd, &st);

  void *buf = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  munmap(buf, st.st_size);
}
