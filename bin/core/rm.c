#include <errno.h>
#include <stdio.h>
#include <sys/sysbind.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
  int res = 0;
  for (int i = 1; i < argc; i++) {
    if (unlink(argv[i]) < 0) {
      int e = errno;
      fprintf(stderr, "Failed to remove '%s', %s\n", argv[i], strerror(e));
      res = 1;
    }
  }
  return res;
}
