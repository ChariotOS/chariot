#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gfx/image.h>
#include <errno.h>
#include <sys/sysbind.h>

const char opts[] = "";
static int flags = 0;

static void usage() {
  fprintf(stderr, "usage: pngi [OPTION]... FILE...\n");
  return;
}

int pngi(const char *path) {
  auto start = sysbind_gettime_microsecond();
  auto bmp = gfx::load_png(path);
  auto end = sysbind_gettime_microsecond();

  if (!bmp) return -ENOENT;


  float aspect_ratio = (float)bmp->width() / (float)bmp->height();

  printf("%s:\n", path);
  printf("  Width: %dpx\n", bmp->width());
  printf("  Height: %dpx\n", bmp->height());
  printf("  Ratio: %f\n", aspect_ratio);
  printf("  Decode Time: %.3fms\n", (end - start) / 1000.0f);


  int size = 80;
  int w = size * aspect_ratio;
  int h = size / aspect_ratio * 0.8;  // *0.8 to account for how chars are taller than they are wide
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float sx = x / (float)w;
      float sy = y / (float)h;
      uint32_t c = bmp->sample(sx, sy, gfx::bitmap::SampleMode::Nearest);
      int r = (c >> 16) & 0xFF;
      int g = (c >> 8) & 0xFF;
      int b = (c >> 0) & 0xFF;
      printf("\e[48;2;%d;%d;%dm ", r, g, b);
    }
    // reset everything and move on to the next line
    printf("\e[0m\n");
  }

  return 0;
}

int main(int argc, char **argv) {
  int ch;
  while ((ch = getopt(argc, argv, opts)) != -1) {
  }

  pngi("/usr/res/lumen/wallpaper.png");


  argc -= optind;
  argv += optind;
  if (argc == 0) {
    usage();
    return EXIT_FAILURE;
  }


  for (int i = 0; i < argc; i++) {
    int res = pngi(argv[i]);
    if (res != 0) exit(EXIT_FAILURE);
  }
  return 0;
}
