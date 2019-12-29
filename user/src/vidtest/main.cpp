#include <chariot.h>
#include <chariot/mouse_packet.h>
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

void write_pixel(int fd, int x, int y, int color) {
  if (buffer == 0) return;
  if (x >= width || y >= height) return;

  int i = y * width + x;
  lseek(fd, i * sizeof(int), SEEK_SET);
  write(fd, &color, sizeof(color));
}

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

int abs(int n) {
  if (n < 0) return -n;
  return n;
}

double floor(double x) {
  return (uint64_t)x;
}

double round(double x) {
  double y, r;

  /* Largest integer <= x */
  y = floor(x);

  /* Fractional part */
  r = x - y;

  /* Round up to nearest. */
  if (r > 0.5) goto rndup;

  /* Round to even */
  if (r == 0.5) {
    r = y - 2.0 * floor(0.5 * y);
    if (r == 1.0) {
    rndup:
      y += 1.0;
    }
  }

  /* Else round down. */
  return (y);
}

void draw_line(int x0, int y0, int x1, int y1, int col) {
  double dx = x1 - x0;
  double dy = y1 - y0;
  double stepCount = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
  double stepX = dx / stepCount;
  double stepY = dy / stepCount;
  double x = x0;
  double y = y0;
  for (int i = 0; i < stepCount + 1; i++) {
    int drawx = (int)round(x);
    int drawy = (int)round(y);
    set_pixel(drawx, drawy, col);
    x = x + stepX;
    y = y + stepY;
  }
}

int main() {
  int fb = open("/dev/fb", O_RDWR);
  int mouse = open("/dev/mouse", O_RDONLY);

  int sb16 = open("/dev/sb16", O_WRONLY);


  printf("sb16=%d\n", sb16);
  char buf[512];

  for (int i = 0; i < 512; i++) {
    buf[i] = i;
  }

  write(sb16, buf, 512);

  buffer = (unsigned int *)calloc(sizeof(int), 640 * 480);

  int mx = 100;
  int my = 100;

  struct mouse_packet pkt;
  while (1) {
    int n = read(mouse, &pkt, sizeof(pkt));
    if (n != sizeof(pkt)) continue;

    mx += pkt.dx;
    my -= pkt.dy;

    if (mx < 0) mx = 0;
    if (my < 0) my = 0;

    if (mx >= 640) mx = 640 - 1;
    if (my >= 480) my = 480 - 1;

    memset(buffer, 0x00, sizeof(int) * 640 * 480);

    int mouse_sz = 16;
    for (int y = 0; y < mouse_sz; y++) {
      for (int x = 0; x < mouse_sz; x++) {
        set_pixel(mx + x, my + y, 0xFFFFFF);
      }
    }

    draw_line(width/2, height/2, mx, my, 0xFF0000);

    lseek(fb, 0, SEEK_SET);
    display_video(fb, buffer, 640, 480);
    continue;
  }

  int c = 0;
  int w = 255;
  while (1) {
    float h = (c % w) / (float)w;
    int color = hsl(h, 1.0, 0.5);
    for (int i = 0; i < 640 * 480; i++) {
      buffer[i] = color;
    }

    int c = -1;
    display_video(fb, buffer, 640, 480);
    c++;
  }
}
