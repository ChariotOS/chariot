#include <chariot/mshare.h>
#include <gfx/bitmap.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sysbind.h>
#include <unistd.h>



void *mshare_create(const char *name, size_t size) {
  struct mshare_create arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)sysbind_mshare(MSHARE_CREATE, &arg);
}


void *mshare_acquire(const char *name, size_t size) {
  struct mshare_acquire arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)sysbind_mshare(MSHARE_ACQUIRE, &arg);
}


static unsigned long read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}

static ck::string unique_ident(void) {
  static long next_id = 0;
  char buf[50];
  snprintf(buf, 50, "%d:%ld-%lx", getpid(), next_id++, read_timestamp());

  return buf;
}



gfx::bitmap::bitmap(size_t w, size_t h) : m_width(w), m_height(h) {
  m_pixels = (uint32_t *)mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
}


gfx::bitmap::bitmap(size_t w, size_t h, uint32_t *buf) {
  m_width = w;
  m_height = h;
  m_pixels = buf;
  m_owned = false;  // we don't own the pixel buffer
}

gfx::bitmap::~bitmap(void) {
  if (m_pixels && m_owned) {
    munmap(m_pixels, size());
    m_pixels = 0;
  }
}


gfx::shared_bitmap::shared_bitmap(size_t w, size_t h) : m_name(unique_ident()) {
  m_height = h;
  m_width = w;
  m_pixels = (uint32_t *)mshare_create(shared_name(), size());
  m_original_size = size();
}

gfx::shared_bitmap::shared_bitmap(const char *name, uint32_t *pix, size_t w, size_t h) : m_name(name) {
  m_pixels = pix;
  m_width = w;
  m_height = h;
  m_original_size = size();
}

ck::ref<gfx::shared_bitmap> gfx::shared_bitmap::get(const char *name, size_t w, size_t h) {
  void *buf = mshare_acquire(name, round_up(w * h * sizeof(uint32_t), 4096));
  if (buf == MAP_FAILED) {
    return nullptr;
  }

  auto bm = ck::make_ref<gfx::shared_bitmap>(name, (uint32_t *)buf, w, h);

  return bm;
}

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define NPAGES(sz) (round_up((sz), 4096) / 4096)
ck::ref<gfx::shared_bitmap> gfx::shared_bitmap::resize(size_t w, size_t h) {
  auto old_pgcount = NPAGES(size());
  auto new_pgcount = NPAGES(w * h * sizeof(uint32_t));
  if (new_pgcount > old_pgcount) {
    // if it's bigger, return a larger "copy"
    return gfx::shared_bitmap::get(shared_name(), w, h);
  } else {
    // if it is smaller, just keep those pages in memory and act like it's the
    // new size
    this->m_width = w;
    this->m_height = h;
  }
  return nullptr;  // ???
}


gfx::shared_bitmap::~shared_bitmap(void) {}



ck::ref<gfx::bitmap> gfx::bitmap::scale(int w, int h, gfx::bitmap::SampleMode m) {
  auto n = ck::make_ref<gfx::bitmap>(w, h);

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float fx = x / (float)w;
      float fy = y / (float)h;
      n->set_pixel(x, y, sample(fx, fy, m));
    }
  }
  return n;
}


uint32_t gfx::bitmap::sample(float fx, float fy, gfx::bitmap::SampleMode m) {
  switch (m) {
    case gfx::bitmap::SampleMode::Nearest:
      return get_pixel(width() * fx, height() * fy);
      break;

    case gfx::bitmap::SampleMode::Bilinear:
      return get_pixel(width() * fx, height() * fy);
      break;
  };


  return 0;
}
