#include <stdio.h>
#include <stdlib.h>
#include <sys/kshell.h>
#include <sys/syscall.h>

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "usage: ksh <command> [args...]\n");
    exit(1);
  }


	return kshell(argv[1], argc - 2, argv + 2, 0, 0);
}
