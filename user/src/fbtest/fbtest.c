#include <chariot/fb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main() {
  int fb;

  fb = open("/dev/fb", O_RDWR);

  if (fb < 0) {
    perror("open(fb)");
    exit(EXIT_FAILURE);
  }

  struct ck_fb_info info = {
      .active = 1,
      .width = 1024,
      .height = 768,
  };

	int w = info.width;
	int h = info.height;

  ioctl(fb, FB_SET_INFO, &info);



	int *pixels = calloc(w * h, sizeof(int));


	int x = w / 2;
	int y = h / 2;
	pixels[y * w + x] = 0xFFFFFF;

	write(fb, pixels, w * h * sizeof(int));

	free(pixels);

  fprintf(stderr, "Send any key to exit\n");
  // wait
  fgetc(stdin);

  exit(EXIT_SUCCESS);
}
