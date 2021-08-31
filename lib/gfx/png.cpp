#include <ck/io.h>
#include <ck/resource.h>
#include <gfx/image.h>
#include <sys/mman.h>
#include <time.h>

// I am lazy, just using an existing png loader
#include "spng/spng.h"


template <typename T>
static auto load_png(void *data, size_t size) {
  // time_t start = clock();
  int r;
  spng_ctx *ctx = NULL;
  struct spng_ihdr ihdr;
  const char *clr_type_str = "Unknown";
  struct spng_row_info row_info = {0};

  T *bmp = NULL;

  ctx = spng_ctx_new(0);

  if ((r = spng_set_png_buffer(ctx, data, size)) != 0) {
    printf("error: %s\n", spng_strerror(r));
    goto error;
  }

  if ((r = spng_get_ihdr(ctx, &ihdr)) != 0) {
    printf("error: %s\n", spng_strerror(r));
    goto error;
  }

  bmp = new T(ihdr.width, ihdr.height);
  spng_decode_image(ctx, bmp->pixels(), bmp->size(), SPNG_FMT_RGBA8, SPNG_DECODE_TRNS);

  // they give us rgba8, we need arg8 because we are sane.
  for (off_t i = 0; i < bmp->width() * bmp->height(); i++) {
    uint8_t *rgba = (uint8_t *)&bmp->pixels()[i];
    uint8_t r = rgba[0];
    uint8_t g = rgba[1];
    uint8_t b = rgba[2];
    uint8_t a = rgba[3];
    bmp->pixels()[i] = (a << 24) | (r << 16) | (g << 8) | (b << 0);
  }

error:
  spng_ctx_free(ctx);
  // printf("spng: %ums\n", clock() - start);

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
  auto map = stream.mmap();

  if (!map) {
    fprintf(stderr, "gfx::load_png: Failed to mmap '%s'\n", path.get());
    return nullptr;
  }

  return load_png<T>(map->data(), map->size());
}


ck::ref<gfx::bitmap> gfx::load_png(ck::string path) { return ::load_png<gfx::bitmap>(path); }


ck::ref<gfx::shared_bitmap> gfx::load_png_shared(ck::string path) {
  return ::load_png<gfx::shared_bitmap>(path);
}


ck::ref<gfx::bitmap> gfx::load_png_from_res(ck::string path) {
  auto opt = ck::resource::get(path.get());

  if (!opt) return {};

  auto [data, sz] = opt.unwrap();
  if (data != NULL) {
    return ::load_png<gfx::bitmap>(data, sz);
  }
  return {};
}
