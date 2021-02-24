#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysbind.h>

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "mkdir NAME...\n");
    exit(EXIT_FAILURE);
  }


  for (int i = 1; i < argc; i++) {
    int res = sysbind_mkdir(argv[i], 0755);
		printf("res: %d\n", res);
    if (res < 0) {
      fprintf(stderr, "mkdir: %s\n", strerror(-res));
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}
