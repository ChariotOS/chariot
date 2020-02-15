#include <unistd.h>
#include <chariot.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++) close(i + 1);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);

  printf("[init] hello, friend\n");

  char *shell = "/bin/sh";
  char *sh_argv[] = {shell, NULL};

  // TODO: load default env from /etc/environ or something
  char **envp = NULL;

  while (1) {
    pid_t sh_pid = spawn();
    if (startpidve(sh_pid, shell, sh_argv, envp) != 0) {
      printf("failed to spawn shell\n");
      return -1;
    }

    while (1) {
      if (waitpid(-1, NULL, 0) == sh_pid) {
        printf("[init] sh died\n");
        break;
      }
    }
  }
}

