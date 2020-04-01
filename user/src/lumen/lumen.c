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

static void set_pixel(int x, int y, int c) {
  if (x > WIDTH || x < 0 || y > HEIGHT || y < 0) return;
  buffer[x + y * WIDTH] = c;
}

void draw_window(struct window *win) {
  int x, y;

  for (y = 0; y < win->pos.h; y++) {
    for (x = 0; x < win->pos.w; x++) {
      set_pixel(x + win->pos.x, y + win->pos.y, 0x252525);
    }
  }
}



static uint32_t seed;  // The state can be seeded with any value.
// Call next() to get 32 pseudo-random bits, call it again to get more bits.
// It may help to make this inline, but you should see if it benefits your
// code.
static inline uint64_t next_random(void) {
  uint32_t z = (seed += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}
static void compose(void) {

	static int i = 0;
  // set it to some kind of grey
  memset(buffer, i++, BSIZE);

  struct window win;

  // arbitrary
  win.pos.w = 160;
  win.pos.h = 195;

  win.pos.x = next_random() % WIDTH;
  win.pos.y = next_random() % HEIGHT;

  draw_window(&win);

  blit();
}

