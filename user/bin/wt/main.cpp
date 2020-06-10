#include <stdio.h>
#include <stdlib.h>
#include <ck/io.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
	int fd = open("/dev/fb", O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}


	int pixels = 640 * 480;

	auto buf = (char*)mmap(NULL, pixels * sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return 1;
	}


	for (int i = 0; i < 4096; i++) {
		memset(buf, i, pixels * sizeof(int));
	}



	return 0;

	/*

	ck::file file("/small", "rw");

	file.seek(2 * 4096, SEEK_END);

	int o = 0;
#define COL  30
#define ROW 20
	for (int i = 0; i < ROW * COL; i++) {
		file.writef("%02x ", i & 0xFF);
		o++;


		if (o == COL) {
			file.writef("\n");
			o = 0;
		}
	}
	file.writef("\n");

	return 0;
	*/
}
