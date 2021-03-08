#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  unsigned long args[7];
  if (argc != 2) {
    fprintf(stderr, "usage: sysfuzz <SYSCALL NR>\n");
    exit(EXIT_FAILURE);
  }

  srand(0);
  args[0] = atoi(argv[1]);


  for (int i = 0; 1; i++) {
    printf("iter %4d ", i);
    for (int i = 1; i < 7; i++) {
      args[i] = rand();
      printf("%08llx ", args[i]);
    }
    printf("\n");

    if (fork() == 0) {
      syscall(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
			exit(0);
    }

		waitpid(-1, NULL, 0);

  }
  return 0;
}
