/**
 * the chariot windowing system - lumen
 */
#include <chariot/fb.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct rect {
  int x, y;
  int w, h;
};


struct window {
	struct rect pos;

	// Z ordering
	struct window *next;
	struct window *prev;
};
static int fbfd = -1;

static uint64_t frame = 0;
static struct rect screen_size;
static uint32_t *buffer = NULL;




#define WIDTH (screen_size.w)
#define HEIGHT (screen_size.h)
#define BSIZE (WIDTH * HEIGHT * sizeof(*buffer))

static void compose(void);

int main(int argc, char **argv) {
  fbfd = open("/dev/fb", O_RDWR);
  if (fbfd < 0) {
    perror("Framebuffer");
    exit(EXIT_FAILURE);
  }

  screen_size.x = 0;
  screen_size.y = 0;
  screen_size.w = 1024;
  screen_size.h = 768;

  struct ck_fb_info fbinfo;
  fbinfo.active = 1;
  fbinfo.width = WIDTH;
  fbinfo.height = HEIGHT;

  ioctl(fbfd, FB_SET_INFO, &fbinfo);

  buffer = calloc(sizeof(uint32_t), WIDTH * HEIGHT);

  while (1) {
    compose();

    frame++;
  }

  free(buffer);

  return EXIT_SUCCESS;
}

static void blit(void) { write(fbfd, buffer, BSIZE); }



static void compose(void) {
  // go through and draw the background
  if (1) {
    long x = 0;
    for (unsigned long i = 0; i < WIDTH * HEIGHT; i++) {
			int color = x & 1 ? 0xFFFFFFFF : 0;
			buffer[i] = color;

			x++;
			if (x > WIDTH) {
				x = 0;
			}

    }
  }




  blit();
}

