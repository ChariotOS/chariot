#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <chariot.h>



int main(int argc, char **argv) {
	if (argc == 2) {
		int code = 0;
		sscanf(argv[1], "%d", &code);
		exit(code);
	}
  exit(0);
  return 0;
}
