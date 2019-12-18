#include <chariot.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

unsigned int *buffer;
int width = 640;
int height = 480;

void set_pixel(int x, int y, int color) {
  if (buffer == 0) return;
  if (x >= width || y >= height) return;
  buffer[y * width + x] = color;
}

void display_video(int fb, unsigned int *buffer, int w, int h) {
  // lseek(fb, 0, SEEK_SET);
  write(fb, buffer, w * h * sizeof(int));
}

static int rgb(int r, int g, int b) {
  return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

static double hue2rgb(double p, double q, double t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1.0 / 6.0) return p + (q - p) * 6 * t;
  if (t < 1.0 / 2.0) return q;
  if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
  return p;
}

static int hsl(double h, double s, double l) {
  double r, g, b;
  if (s == 0) {
    r = g = b = l;  // achromatic
  } else {
    double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    double p = 2.0 * l - q;
    r = hue2rgb(p, q, h + 1 / 3.0);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0 / 3.0);
  }
  return rgb(r * 255.0, g * 255.0, b * 255.0);
}

struct window {
  struct window *next;
  int x, y;
  int w, h;
  int color;
};

void draw_window(struct window *w) {
  for (int y = w->y; y < w->h; y++) {
    for (int x = w->x; x < w->w; x++) {
      set_pixel(x, y, w->color);
    }
  }
}

uint32_t x;
inline uint32_t rand(void) {
  uint32_t z = (x += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

int main() {
  // TODO: inherit from parent task
  open("/dev/console", O_RDWR);

  int fb = open("/dev/fb", O_RDWR);

  /*
  FILE *font = fopen("/usr/res/fonts/cherry.bdf", "r");

  if (font != NULL) {

    auto c = (char*)malloc(64);

    fread(c, 64, 1, font);

    c[63] = 0;
    printf("%s\n", c);
  }
  */



  buffer = (unsigned int*)calloc(sizeof(int), 640 * 480);


  int c = 0;
  int w = 255;
  while (1) {
    float h = (c % w) / (float)w;
    int color = hsl(h, 1.0, 0.5);
    for (int i = 0; i < 640 * 480; i++) {
      buffer[i] = color;
    }
    display_video(fb, buffer, 640, 480);
    c++;
  }
}
