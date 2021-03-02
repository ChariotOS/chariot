#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void segv_handler(int sig) {
	printf("segfaulted!\n");

	int *x = NULL;

	// will always segfault
	*x = 0;
	exit(EXIT_FAILURE);
}

int main() {
	int *x = NULL;

	signal(SIGSEGV, segv_handler);

	// will always segfault
	*x = 0;
	return 0;
}
