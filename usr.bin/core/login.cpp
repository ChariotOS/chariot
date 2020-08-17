#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main() {

	// TODO: actually login lol
	while (1) {
		system("cat /cfg/motd");
		system("/bin/sh");
		printf("[login] Restarting shell...\n");
	}
	return 0;
}
