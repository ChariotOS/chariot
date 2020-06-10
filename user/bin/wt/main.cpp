#include <stdio.h>
#include <stdlib.h>
#include <ck/io.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
	int fd = open("/small", O_RDWR);

	auto buf = (char*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	buf[0]++;
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
