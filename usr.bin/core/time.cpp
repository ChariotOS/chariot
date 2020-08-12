#include <ck/command.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>
#include <sys/wait.h>
#include <time.h>

static size_t current_ms() { return sysbind_gettime_microsecond() / 1000; }


int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "time: no command provided\n");
    exit(-1);
  }


  auto cmd = ck::command(argv[1]);
  for (int i = 2; i < argc; i++) cmd.arg(argv[i]);


  auto start = current_ms();
  int res = cmd.exec();
  auto end = current_ms();

  fprintf(stderr, "\n--------------------\n");
  fprintf(stderr, "%ldms\n", end - start);

  return WEXITSTATUS(res);
}
