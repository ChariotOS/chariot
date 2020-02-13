#pragma once

struct bdf_font;

struct bdf_font *bdf_load(const char *path);
void bdf_close(struct bdf_font *);

struct bdf_scribe_cmd {};
void bdf_scribe(struct bdf_font *fnt, const char *msg, int *dx, int *dy,
                unsigned *image, int width, int height, unsigned fg,
                unsigned bg);
