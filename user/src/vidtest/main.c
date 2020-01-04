#include <chariot.h>
#include <chariot/mouse_packet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "teapot.h"

void hexdump(void *vbuf, long len) {
  unsigned char *buf = vbuf;

  int w = 16;
  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;
    for (int c = 0; c < w; c++) {
      if (i + c > len) {
        printf("   ");
      } else {
        printf("%02X ", line[c]);
      }
    }
    printf(" |");
    for (int c = 0; c < w; c++) {
      if (i + c > len) {
      } else {
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    printf("|\n");
  }
}

struct bmp_header {
  char header[2];   // "BM"
  unsigned size;    // 4 bytes
  unsigned resv;    // 4 bytes
  unsigned offset;  // 4 bytes, where the data starts


  unsigned int bi_size;
  int width;
  int height;
  uint16_t planes;
  uint16_t pix;
  unsigned int compression;
  unsigned int size_image;
  int biXPelsPerMeter;
  int biYPelsPerMeter;
  unsigned int clr_used;
  unsigned int clr_important;
} __attribute__((packed));

struct bitmap {
  struct bmp_header hdr;

  char *data;

  unsigned *pixels;
};

struct bitmap *open_bitmap(const char *path) {
  struct stat st;
  int fd = 0;
  void *fdata = NULL;
  struct bitmap *bmp = NULL;

  fd = open(path, O_RDONLY);
  if (fd < 0) return NULL;

  if (fstat(fd, &st) < 0) goto fail;

  bmp = calloc(sizeof(*bmp), 1);
  fdata = malloc(st.st_size);
  if (bmp == NULL || fdata == NULL) goto fail;

  bmp->data = fdata;

  if (read(fd, fdata, st.st_size) != st.st_size) goto fail;

  // read the header into the struct so we can use it
  memcpy(&bmp->hdr, fdata, sizeof(bmp->hdr));
  bmp->pixels = (unsigned *)((char *)fdata + bmp->hdr.offset);

  printf("width=%d, height=%d\n", bmp->hdr.width, bmp->hdr.height);

  return bmp;

fail:
  if (fd >= 0) close(fd);
  if (fdata != NULL) free(fdata);
  if (bmp != NULL) free(bmp);
  return NULL;
}

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
  if (x < 0 || y < 0) return;
  if (x >= width || y >= height) return;
  buffer[y * width + x] = color;
}

void display_video(int fb, unsigned int *buffer, int w, int h) {
  lseek(fb, 0, SEEK_SET);
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

double floor(double x) { return (uint64_t)x; }

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

double fabs(double n) {
  if (n < 0) return -n;
  return n;
}

void draw_line(int x0, int y0, int x1, int y1, int col) {
  double dx = x1 - x0;
  double dy = y1 - y0;
  double stepCount = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
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

// matrix and vector types
typedef union {
  struct {
    float x, y;
  };
  float m[2];
} vec2;
typedef float mat2[2][2];

typedef float vec3[3];
typedef float mat3[3][3];

typedef float vec4[4];
typedef float mat4[4][4];

int main() {
  // display a 2d teapot from an obj file
  float x, y, z;
  int fb = open("/dev/fb", O_RDWR);
  buffer = calloc(sizeof(int), width * height);

  x = y = z = 0;

  float scale = 100;

  int tp_len = sizeof(teapot_verts) / sizeof(teapot_verts[0]);

  struct bitmap *bmp = open_bitmap("/misc/cat.bmp");

  printf("bmp=%p\n", bmp);

  while (1) {
    memset(buffer, 0, width * height * sizeof(unsigned));
    for (int i = 0; i < tp_len; i += 3) {
      x = teapot_verts[i];
      y = -teapot_verts[i + 1];
      z = teapot_verts[i + 2];

      int jitter = 12;
      int dx = x * scale + (width / 2) + (rand() % (jitter * 2) - jitter);
      int dy = y * scale + (height / 1.2) + (rand() % (jitter * 2) - jitter);

      set_pixel(dx, dy, 0xFF00FF);
    }
    display_video(fb, buffer, width, height);
  }

  free(buffer);
}
