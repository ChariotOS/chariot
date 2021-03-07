#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

volatile int ready = 0;


static void sigusr1(int i) {
  ready = 1;
}

int main() {
  pid_t parent = getpid();
  signal(SIGUSR1, sigusr1);


  pid_t pid = fork();
  if (pid == 0) {
    printf("hello from child!\n");
    kill(parent, SIGUSR1);
    while (1) {
      sched_yield();
    }
  }

  while (!ready)
    sched_yield();
  kill(pid, SIGKILL);

  int status = 0;
  waitpid(pid, &status, 0);

  printf("exit code in parent: %d\n", WEXITSTATUS(status));
  return 0;
}
