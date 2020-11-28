#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_usage_and_exit() {
  printf("usage: kill [-signal] <PID>\n");
  exit(1);
}

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) print_usage_and_exit();

  unsigned signum = SIGTERM;
  int pid_argi = 1;
  if (argc == 3) {
    pid_argi = 2;
    if (argv[1][0] != '-') print_usage_and_exit();

    int s = 0;
    if (sscanf(&argv[1][1], "%d", &s) == 0) print_usage_and_exit();
    signum = s;
  }


  pid_t pid = -1;
  if (sscanf(argv[pid_argi], "%d", &pid) == 0) print_usage_and_exit();
  int rc = kill(pid, signum);
  if (rc < 0) perror("kill");
  return 0;
}
