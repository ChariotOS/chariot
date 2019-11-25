#include <chariot.h>
#include <stdio.h>
#include <stdlib.h>

void fail() {
  while (1) {
  }
}

int main(int argc, char **argv) {
  int pid = spawn();

  if (pid == -1) {
    printf("failed to spawn!\n");
  }
  printf("starting test\n");
  char *cmd[] = {"/bin/test", 0};
  int res = cmdpidv(pid, cmd[0], cmd);
  if (res != 0) {
    printf("failed to cmd, reason = %d\n", res);
  }

  while (1) {
  }
}

