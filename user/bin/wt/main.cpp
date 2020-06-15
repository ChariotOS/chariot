#include <ck/io.h>

int main(int argc, char **argv) {

	ck::file f("/small", "r+");


	f.writef("hello, world\n");

	return 0;
}
