#include <stdio.h>
#include <stdlib.h>
#include <ck/io.h>
#include <string.h>

int main(int argc, char **argv) {

	ck::file file("/small", "rw");

	file.seek(0, SEEK_END);


	for (int i = 0; i < 20; i++) {
		int n = file.writef("(%d bytes so far)\n", file.size());
		printf("wrote %d\n", n);
	}

	return 0;
}
