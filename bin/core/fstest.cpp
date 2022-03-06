#include <ck/io.h>
#include <unistd.h>
#include <fcntl.h>


void expect(const char *msg, bool val) {
  if (val) {
    fprintf(stderr, "\e[0;32m[PASS]\e[0m %s\n", msg);
  } else {
    fprintf(stderr, "\e[0;31m[FAIL]\e[0m %s\n", msg);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "fstest: provided no paths\n");
    exit(EXIT_FAILURE);
  }
  {
    ck::file f;
    expect("Open", f.open(argv[1], O_RDWR | O_CREAT, 0644));
  }

  expect("Unlink", unlink(argv[1]) == 0);



  return 0;
}
