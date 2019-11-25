#include <chariot.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int pid = spawn();
  if (pid != -1) {
    char *cmd[] = {"/bin/test", 0};

    int res = cmdpidv(pid, cmd[0], cmd);
    if (res != 0) {
      printf("failed to cmd, reason = %d\n", res);
    }
  }

  while (1) {
  }
}

