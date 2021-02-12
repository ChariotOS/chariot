#include <chariot/mountopts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysbind.h>

int mount(const char *dev, const char *dir, const char *type, unsigned long flags, void *data) {
  struct mountopts opts;
  strncpy(opts.device, dev, sizeof(opts.device));
  strncpy(opts.dir, dir, sizeof(opts.dir));
  strncpy(opts.type, type, sizeof(opts.type));
  opts.flags = flags;
  opts.data = data;
  return sysbind_mount(&opts);
}

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: mount <dev> <dir> <type>\n");
    return EXIT_FAILURE;
  }

  return mount(argv[1], argv[2], argv[3], 0, NULL);
}
