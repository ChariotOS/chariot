#include "bdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct bdf_glyph {
  int valid;
  int dw, dh;  // device size
  int gw, gh;  // glyph size
  int ox, oy;  // offset

  unsigned *data;
};

struct bdf_font {
  FILE *fp;

  struct {
    int major;
    int minor;
  } version;

  struct {
    int a[2];
    int b[2];
  } bb;

  int xppi;
  int yppi;
  int point;
  int chars;

  unsigned raw_size;
  char *raw_data;

  int width, height;

  struct bdf_glyph glyphv[256];
};

static int bdf_parse(struct bdf_font *);

struct bdf_font *bdf_load(const char *path) {
  struct stat st;
  if (lstat(path, &st) != 0) {
    return NULL;
  }

  FILE *fp = fopen(path, "r");
  if (fp == NULL) return NULL;

  struct bdf_font *fnt = malloc(sizeof(*fnt));
  memset(fnt, 0, sizeof(*fnt));
  fnt->fp = fp;

  fnt->raw_data = malloc(st.st_size + 1);
  fread(fnt->raw_data, st.st_size, 1, fnt->fp);
  // null terminate
  fnt->raw_data[st.st_size] = 0;
  fnt->raw_size = st.st_size;

  bdf_parse(fnt);

  return fnt;
}

void bdf_close(struct bdf_font *fnt) {
  for (int i = 0; i < 256; i++) {
    // TODO: free the pixel buffer
  }

  free(fnt);
}

static char *next_line(char *from) {
  while (1) {
    if (*from == '\0') return NULL;
    if (*from == '\n') break;
    from++;
  }

  return from + 1;
}

static int starts_with(char *string, char *prefix) {
  while (*prefix)
    if (*prefix++ != *string++) return 0;
  return 1;
}

#define STATE_ROOT 0
#define STATE_CHAR 1
#define STATE_DONE 2

int bdf_parse(struct bdf_font *f) {
  char *data = f->raw_data;

  struct bdf_glyph invalid_glyph;

  struct bdf_glyph *g = &invalid_glyph;

#define STARTCASE() if (0)
#define CASE(str) if (starts_with(data, str))

#define HANDLE(cmd, fmt, dsts...)       \
  if (starts_with(data, cmd)) {         \
    sscanf(data, cmd fmt "\n", ##dsts); \
    goto done;                          \
  }

#define TRANSITION(cmd, st)          \
  if (starts_with(data, cmd)) {      \
    printf("%d -> %d\n", state, st); \
    state = st;                      \
    goto done;                       \
  }

  for (int l = 0; data != NULL; l++) {
    HANDLE("SIZE ", "%d %d %d", &f->point, &f->xppi, &f->yppi);
    HANDLE("FONTBOUNDINGBOX ", "%d %d %d %d", &f->bb.a[0], &f->bb.a[1],
           &f->bb.b[0], &f->bb.b[1]);

    CASE("ENCODING") {
      int c = 0;
      sscanf(data, "ENCODING %d\n", &c);
      g = &f->glyphv[c];
      goto done;
    }

    CASE("BBX ") {
      // printf("bbx\n");
      sscanf(data, "BBX %d %d %d %d\n", &g->gw, &g->gh, &g->ox, &g->oy);
      // printf("BBX %d %d %d %d\n", g->gw, g->gh, g->ox, g->oy);
      goto done;
    }

    if (starts_with(data, "BITMAP")) {
      // printf("bitmap\n");

      g->valid = 1;
      g->data = malloc(sizeof(unsigned) * g->gh);

      // printf("gh=%d\n", g->gh);
      for (int i = 0; i < g->gh; i++) {
        data = next_line(data);
        sscanf(data, "%x\n", g->data + i);
        // printf("%02x\n", g->data[i]);
      }

      goto done;
    }

  done:

    data = next_line(data);
  }

  return 0;
}

void bdf_scribe(struct bdf_font *fnt, const char *msg, int *dx, int *dy,
                unsigned *image, int width, int height, unsigned fg,
                unsigned bg) {
  int x = *dx;
  int y = *dy;

#define setpixel(x, y, c) \
  if ((x) >= 0 && (x) < width && (y) >= 0 && (y) < height) image[(x) + (y) * width] = (c)

  printf("fnt=%p\n", fnt);
  for (int i = 0; msg[i] != '\0'; i++) {
    char c = msg[i];
    struct bdf_glyph *g = &fnt->glyphv[(int)c];
    if (!g->valid) continue;

    printf("'%c'\n", c);

    for (int ox = 0; ox < g->gw; ox++) {
      for (int oy = 0; oy < g->gh; oy++) {
        setpixel(x+ox, y+oy, 0x000000);
      }
    }

    x += g->gw;
  }

  *dx = x;
  *dy = y;
}
