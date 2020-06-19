#include <ck/io.h>
#include <gfx/image.h>
#include <sys/mman.h>

// I am lazy, just using an existing png loader
#include "pngle/pngle.h"




void draw_pixel(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                uint8_t rgba[4]) {
	auto *bmp = (gfx::bitmap*)pngle_get_user_data(pngle);


  uint8_t r = rgba[0];
  uint8_t g = rgba[1];
  uint8_t b = rgba[2];
  uint8_t a = rgba[3];

	uint32_t pix = (a << 24) | (r << 16) | (g << 8) | (b << 0);

	bmp->set_pixel(x, y, pix);
}

void init_screen(pngle_t *pngle, uint32_t w, uint32_t h) {
	pngle_set_user_data(pngle, (void*)new gfx::bitmap(w, h, false));
	return;
}

void flush_screen(pngle_t *pngle) {
	return;
}



ck::ref<gfx::bitmap> gfx::load_png(ck::string path) {
  auto stream = ck::file(path, "r+");
  if (!stream) {
    fprintf(stderr, "gfx::load_png: Failed to open '%s'\n", path.get());
    return nullptr;
  }

  // map the file into memory and parse it for files
  size_t size = stream.size();
  auto raw = (unsigned char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE,
                                   stream.fileno(), 0);


  if (raw == MAP_FAILED) {
    fprintf(stderr, "gfx::load_png: Failed to mmap '%s'\n", path.get());
    return nullptr;
  }

  pngle_t *pngle = pngle_new();

  pngle_set_init_callback(pngle, init_screen);
  pngle_set_draw_callback(pngle, draw_pixel);
  pngle_set_done_callback(pngle, flush_screen);
  // pngle_set_display_gamma(pngle, display_gamma);

	pngle_feed(pngle, raw, size);

  munmap(raw, size);

  auto bmp = (gfx::bitmap*)pngle_get_user_data(pngle);

	free(pngle);


  return bmp;
}
