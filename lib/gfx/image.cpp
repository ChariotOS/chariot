#include <gfx/image.h>
#include <ck/io.h>
#include <sys/sysbind.h>

#define STBI_NO_THREAD_LOCALS

// extern "C" {
#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"
// }



ck::ref<gfx::bitmap> gfx::load_image(ck::string path) {
	// printf("loading %s...\n", path.get());
	auto start = sysbind_gettime_microsecond();
  ck::file f;
  if (!f.open(path, "r")) return nullptr;
  int width, height, channels;


  auto m = f.mmap();
  uint32_t buf[4] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0};
  unsigned char *img =
      stbi_load_from_memory((unsigned char *)m->data(), m->size(), &width, &height, &channels, 4);
  if (img == NULL) return nullptr;
  // printf("img: %p, width: %d, height: %d, channels: %d\n", img, width, height, channels);

	auto end = sysbind_gettime_microsecond();


  auto bmp = ck::make_ref<gfx::bitmap>(width, height);

  unsigned char *src = img;
  unsigned char *dst = (unsigned char *)bmp->pixels();
  for (off_t i = 0; i < width * height * 4; i += 4) {
    // red channel
    dst[i + 2] = src[i + 0];
    // green channel
    dst[i + 1] = src[i + 1];
    // blue channel
    dst[i + 0] = src[i + 2];
    // alpha channel
    dst[i + 3] = src[i + 3];
  }

  // memcpy(bmp->pixels(), img, width * height * 4);

  stbi_image_free(img);

	

	printf("loading %s took %fms\n", path.get(), (end - start) / 1000.0f);

  return bmp;
}
