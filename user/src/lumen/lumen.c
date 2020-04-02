/**
 * the chariot windowing system - lumen
 */
#include <chariot.h>
#include <chariot/fb.h>
#include <fcntl.h>
#include <math.h>
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

  int background_color;
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
      set_pixel(x + win->pos.x, y + win->pos.y, win->background_color);
    }
  }
}

static uint32_t seed;
static inline uint64_t next_random(void) {
  uint32_t z = (seed += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

double sin(double angle) {
  double ret = 0.0;
  __asm__("fsin" : "=t"(ret) : "0"(angle));

  return ret;
}


void draw_circle(int xc, int yc, int x, int y, int color) {
  set_pixel(xc + x, yc + y, color);
  set_pixel(xc - x, yc + y, color);
  set_pixel(xc + x, yc - y, color);
  set_pixel(xc - x, yc - y, color);
  set_pixel(xc + y, yc + x, color);
  set_pixel(xc - y, yc + x, color);
  set_pixel(xc + y, yc - x, color);
  set_pixel(xc - y, yc - x, color);
}

void circleBres(int xc, int yc, int r, int color) {
  int x = 0, y = r;
  int d = 3 - 2 * r;
  draw_circle(xc, yc, x, y, color);
  while (y >= x) {
    // for each pixel we will
    // draw all eight pixels

    x++;

    // check for decision parameter
    // and correspondingly
    // update d, x, y
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    } else
      d = d + 4 * x + 6;
    draw_circle(xc, yc, x, y, color);
		blit();
  }
}

static void compose(void) {
  // set it to some kind of grey
  memset(buffer, 0x00, BSIZE);

#define C(x) ((x & 0xFF) | (x << 8 & 0xFF) | (x << 16 & 0xFF))
	for (int i = 0; i < 200; i++) {
	circleBres(WIDTH/2, HEIGHT/2, i, C(i));
	}

  /*
for (int i = 0; i < 10; i++) {
struct window win;
// arbitrary
win.pos.w = 160;
win.pos.h = 195;
win.pos.x = next_random() % WIDTH;
win.pos.y = next_random() % HEIGHT;

	  win.background_color = next_random();
draw_window(&win);
}
  */

  blit();
  yield();
}

