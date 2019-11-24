#include <chariot.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv) {


  void *p = malloc(500);

  printf("p=%p\n", p);

  free(p);

  while (1) {}

  int pid = spawn();
  if (pid != -1) {
    char *cmd[] = {"/bin/test", 0};
    int res = cmdpidv(pid, cmd[0], cmd);
    if (res != 0) {
      printf("failed to cmd, reason = %d\n", res);
    }

    printf("%d\n", cmdpidv(pid, cmd[0], cmd));
  }

  while (1) {
    // printf("A");
  }
}

