#include <ck/io.h>
#include <unistd.h>
#include <fcntl.h>


void expect(const char *msg, bool val) {
	if (!val) {

	}
}

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "fstest: provided no paths\n");
    exit(EXIT_FAILURE);
  }
	ck::file f;
	expect("open", f.open(argv[1], O_RDWR | O_CREAT, 0644) == 0);

	return 0;
}
