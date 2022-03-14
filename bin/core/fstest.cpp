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

uint8_t buffer[4096 * 8];
uint8_t read_buffer[4096 * 8];

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "fstest: provided no paths\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < sizeof(buffer); i++)
    buffer[i] = rand();

  {
    ck::file f;
    expect("Open", f.open(argv[1], O_RDWR | O_CREAT, 0644));

		// TODO: truncate the file to zero bytes
		// expect("Truncate", ftruncate(f.fileno(), 0) == 0);
    expect("Write", f.write(buffer, sizeof(buffer)) > 0);
		expect("Seek back to 0", f.seek(0, SEEK_SET) == 0);
    expect("Read", f.read(read_buffer, sizeof(read_buffer)) > 0);
		expect("Memory comparison", memcmp(read_buffer, buffer, sizeof(buffer)) == 0);

  }

  expect("Unlink", unlink(argv[1]) == 0);



  return 0;
}
