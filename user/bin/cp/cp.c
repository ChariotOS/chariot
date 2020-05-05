#include <stdio.h>
#include <stdlib.h>

#define BSIZE 1024

int main(int argc, char **argv) {
	// this is a bad cp implementation (temporary)
	if (argc != 3) {
		fprintf(stderr, "Usage: cp <src> <dst>\n");
		exit(EXIT_FAILURE);
	}


	FILE *src = fopen(argv[1], "r");
	FILE *dst = fopen(argv[2], "w");

	if (src == NULL) {
		fprintf(stderr, "cp: src not found\n");
		exit(EXIT_FAILURE);
	}

	if (dst == NULL) {
		fprintf(stderr, "cp: dst not found\n");
		exit(EXIT_FAILURE);
	}

	void *buf = malloc(BSIZE);
	while (!feof(src)) {
		int n = fread(buf, 1, BSIZE, src);
		if (n == 0) break;
		fwrite(buf, 1, n, dst);
	}
	free(buf);
}
