#include <stdio.h>
#include <stdlib.h>
#include <ck/io.h>
#include <string.h>


int main(int argc, char **argv) {

	ck::file file("/small", "rw");

	file.seek(3 * 1024 * 4096, SEEK_SET);
	file.writef("a\n");

	return 0;
}
