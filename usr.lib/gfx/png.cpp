#include <ck/io.h>
#include <gfx/image.h>
#include <sys/mman.h>

// I am lazy, just using an existing png loader
#include <ck/resource.h>
#include "pngle/pngle.h"



template <typename T>
void init_screen(pngle_t *pngle, uint32_t w, uint32_t h) {
  pngle_set_user_data(pngle, (void *)new T(w, h));
}

void flush_screen(pngle_t *pngle) { return; }

void draw_pixel(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
  auto *bmp = (gfx::bitmap *)pngle_get_user_data(pngle);


  uint8_t r = rgba[0];
  uint8_t g = rgba[1];
  uint8_t b = rgba[2];
  uint8_t a = rgba[3];

  uint32_t pix = (a << 24) | (r << 16) | (g << 8) | (b << 0);

  bmp->set_pixel(x, y, pix);
}



template <typename T>
static auto load_png(void *data, size_t size) {
  pngle_t *pngle = pngle_new();

  pngle_set_init_callback(pngle, init_screen<T>);
  pngle_set_draw_callback(pngle, draw_pixel);
  pngle_set_done_callback(pngle, flush_screen);
  // pngle_set_display_gamma(pngle, display_gamma);

  pngle_feed(pngle, data, size);

  auto bmp = (T *)pngle_get_user_data(pngle);

	pngle_destroy(pngle);

  return ck::ref<T>(bmp);
}


template <typename T>
static ck::ref<T> load_png(ck::string path) {
  auto stream = ck::file(path, "r+");
  if (!stream) {
    fprintf(stderr, "gfx::load_png: Failed to open '%s'\n", path.get());
    return nullptr;
  }

  // map the file into memory and parse it for files
  size_t size = stream.size();
  auto raw = (unsigned char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, stream.fileno(), 0);


  if (raw == MAP_FAILED) {
    fprintf(stderr, "gfx::load_png: Failed to mmap '%s'\n", path.get());
    return nullptr;
  }

  return load_png<T>(raw, size);
}


ck::ref<gfx::bitmap> gfx::load_png(ck::string path) { return ::load_png<gfx::bitmap>(path); }


ck::ref<gfx::shared_bitmap> gfx::load_png_shared(ck::string path) { return ::load_png<gfx::shared_bitmap>(path); }


ck::ref<gfx::bitmap> gfx::load_png_from_res(ck::string path) {
  size_t sz = 0;
  void *data = ck::resource::get(path.get(), sz);
  if (data != NULL) {
    return ::load_png<gfx::bitmap>(data, sz);
  }
  return {};
}
