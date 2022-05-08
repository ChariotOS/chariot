#include <sys/ioctl.h>
#include <stdio.h>
#include <chariot/gvi.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: gvi <video device>\n");
    return EXIT_FAILURE;
  }

  int fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    fprintf(stderr, "error: %s\n", strerror(errno));
    return EXIT_FAILURE;  // no need to close()
  }

  uint32_t magic = ioctl(fd, GVI_IDENTIFY);
  if (magic != GVI_IDENTIFY_MAGIC) {
    fprintf(stderr, "error: Not a GVI device\n");
    close(fd);
    return EXIT_FAILURE;
  }

  struct gvi_video_mode mode;

  if (ioctl(fd, GVI_GET_MODE, &mode) != 0) {
    fprintf(stderr, "error: %s\n", strerror(errno));
    close(fd);
    return EXIT_FAILURE;
  }


  printf("Width: %dpx\n", mode.width);
  printf("Height: %dpx\n", mode.height);
  printf("Capabilities:");
  if (mode.caps & GVI_CAP_DOUBLE_BUFFER) printf(" 2buf");
  printf("\n");


  uint32_t bufsz = mode.width * mode.height * sizeof(uint32_t);
  uint32_t *buf = mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);


  for (int k = 0; k < 100; k++) {
    uint32_t color = rand();
    for (int i = 0; i < mode.width * mode.height; i++) {
      buf[i] = color;
    }

    if (mode.caps & GVI_CAP_DOUBLE_BUFFER) ioctl(fd, GVI_FLUSH_FB);
    // usleep(1000 * 16);
  }

  close(fd);
  return 0;
}
