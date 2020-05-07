#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

int main(int argc, char **argv) {

	if (argc != 2) {
		fprintf(stderr, "usage: allocate <size in MB>\n");
		exit(EXIT_FAILURE);
	}



	size_t mb = 0;

	if (sscanf(argv[1], "%zu", &mb) == 0) {
		fprintf(stderr, "invalid number '%s'\n", argv[1]);
		exit(EXIT_FAILURE);
	}


	printf("Allocating %zuMB of ram and writing 1 byte per page...\n", mb);




	size_t size = round_up(mb * 1024 * 1024, 4096);
	char *buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);

	for (off_t i = 0; i < size; i += 4096) {
		buf[i] = 'a';
		printf("\r%.2f%%", (float)i / size * 100.0);
	}

	printf("\nDone!\n");


	munmap(buf, size);


  return 0;
}
