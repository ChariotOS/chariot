#include <signal.h>
#include <stdio.h>
#include <sys/sysbind.h>
#include <unistd.h>

void sigint_handler(int sig) {
  printf("sigint on %d\n", getpid());
}




int main() {
  pid_t pid = fork();

  if (pid == 0) {
    // create a new group for the process
    int res = setpgid(0, 0);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    signal(SIGINT, sigint_handler);
    for (int i = 0; i < 10; i++) {
      if (fork() == 0) break;
    }

    while (1) {
      sysbind_sigwait();
    }
  }


  /* wait a bit */
  usleep(10 * 1000);
  kill(-pid, SIGTERM);
  return 0;
}
