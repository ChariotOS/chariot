#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
	system("cat /cfg/motd");

	while (1) {
		system("/bin/sh");
		printf("[login] Restarting shell...\n");
	}
	return 0;
}
