#include <chariot/mshare.h>
#include <gfx/bitmap.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

void *mshare_create(const char *name, size_t size) {
  struct mshare_create arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)syscall(SYS_mshare, MSHARE_CREATE, &arg);
}


void *mshare_acquire(const char *name, size_t size) {
  struct mshare_acquire arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)syscall(SYS_mshare, MSHARE_ACQUIRE, &arg);
}


unsigned long read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}

static ck::string unique_ident(void) {
  static long next_id = 0;
  char buf[50];
  snprintf(buf, 50, "%d:%d-%llx", getpid(), next_id++, read_timestamp());

  return buf;
}



gfx::bitmap::bitmap(size_t w, size_t h) : m_width(w), m_height(h) {
  m_pixels = (uint32_t *)mmap(NULL, size(), PROT_READ | PROT_WRITE,
                              MAP_ANON | MAP_PRIVATE, -1, 0);
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
  }
}


gfx::shared_bitmap::shared_bitmap(size_t w, size_t h) : m_name(unique_ident()) {
  m_height = h;
  m_width = w;
  m_pixels = (uint32_t *)mshare_create(shared_name(), size());
  m_original_size = size();
}

gfx::shared_bitmap::shared_bitmap(const char *name, uint32_t *pix, size_t w,
                                  size_t h)
    : m_name(name) {
  m_pixels = pix;
  m_width = w;
  m_height = h;
  m_original_size = size();
}

ck::ref<gfx::shared_bitmap> gfx::shared_bitmap::get(const char *name, size_t w,
                                                    size_t h) {
  void *buf = mshare_acquire(name, w * h * sizeof(uint32_t));
  if (buf == MAP_FAILED) {
    return nullptr;
  }

  return new gfx::shared_bitmap(name, (uint32_t *)buf, w, h);
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


gfx::shared_bitmap::~shared_bitmap(void) {
  if (m_pixels) {
    munmap(m_pixels, m_original_size);
  }
}

static inline int abs(int a) { return a < 0 ? -a : a; }
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }




void gfx::bitmap::draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
  // horrizontal optimization
  if (x0 == x1) {
    auto end = min(height(), y1);
    for (int y = max(0, y0); y <= end; y++) {
      set_pixel(x0, y, color);
    }
    return;
  }
  // vertical line optimization
  if (y0 == y1) {
    auto end = min(width(), x1);
    for (int x = max(0, x0); x <= end; x++) {
      set_pixel(x, y0, color);
    }
    return;
  }


  /* Draw a straight line from (x0,y0) to (x1,y1) with given color
   * Parameters:
   *      x0: x-coordinate of starting point of line. The x-coordinate of
   *          the top-left of the screen is 0. It increases to the right.
   *      y0: y-coordinate of starting point of line. The y-coordinate of
   *          the top-left of the screen is 0. It increases to the bottom.
   *      x1: x-coordinate of ending point of line. The x-coordinate of
   *          the top-left of the screen is 0. It increases to the right.
   *      y1: y-coordinate of ending point of line. The y-coordinate of
   *          the top-left of the screen is 0. It increases to the bottom.
   *      color: color value for line
   */
  short steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }

  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  short dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  short err = dx / 2;
  short ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      set_pixel(y0, x0, color);
    } else {
      set_pixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}




void gfx::bitmap::draw_rect(const gfx::rect &r, uint32_t color) {
  // top
  draw_line(r.left(), r.top(), r.right(), r.top(), color);

  // left
  draw_line(r.left(), r.top(), r.left(), r.bottom(), color);
  // right
  draw_line(r.right(), r.top(), r.right(), r.bottom(), color);

  // bottom
  draw_line(r.left(), r.bottom(), r.right(), r.bottom(), color);
}

void gfx::bitmap::fill_rect(const gfx::rect &d, uint32_t color) {
  auto r = gfx::rect(0, 0, width(), height()).intersect(d);
  for (int y = r.top(); y < r.bottom(); y++) {
    for (int x = r.left(); x < r.right(); x++) {
      set_pixel(x, y, color);
    }
  }
}



void gfx::bitmap::draw_rect(const gfx::rect &rect, int r, uint32_t color) {
  int x = rect.left();
  int y = rect.top();
  int h = rect.h;
  int w = rect.w;

  if (r > h >> 1) r = h >> 1;
  if (r > w >> 1) r = w >> 1;
  // smarter version
  draw_hline(x + r, y, w - 2 * r, color);          // Top
  draw_hline(x + r, y + h - 1, w - 2 * r, color);  // Bottom
  draw_vline(x, y + r, h - 2 * r, color);          // Left
  draw_vline(x + w - 1, y + r, h - 2 * r, color);  // Right
  // draw four corners
  draw_circle_helper(x + r, y + r, r, 1, color);
  draw_circle_helper(x + w - r - 1, y + r, r, 2, color);
  draw_circle_helper(x + w - r - 1, y + h - r - 1, r, 4, color);
  draw_circle_helper(x + r, y + h - r - 1, r, 8, color);
}


void gfx::bitmap::fill_rect(const gfx::rect &rect, int r, uint32_t color) {
  int x = rect.left();
  int y = rect.top();
  int h = rect.h;
  int w = rect.w;

  if (r > h >> 1) r = h >> 1;
  if (r > w >> 1) r = w >> 1;

  int delta = w - 2 * r - 1;
  // draw the top round
  fill_circle_helper(x + r, y + r, r, 1, delta, color);
  // draw the middle rect
  fill_rect(gfx::rect(x, y + r, w, h - 2 * r), color);
  // draw the bottom round
  fill_circle_helper(x + r, y + h - r - 1, r, 2, delta, color);
}

void gfx::bitmap::draw_circle(int x0, int y0, int r, uint32_t color) {
  /* Draw a circle outline with center (x0,y0) and radius r, with given color
   * Parameters:
   *      x0: x-coordinate of center of circle. The top-left of the screen
   *          has x-coordinate 0 and increases to the right
   *      y0: y-coordinate of center of circle. The top-left of the screen
   *          has y-coordinate 0 and increases to the bottom
   *      r:  radius of circle
   *      color: 16-bit color value for the circle. Note that the circle
   *          isn't filled. So, this is the color of the outline of the circle
   * Returns: Nothing
   */
  short f = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x = 0;
  short y = r;

  set_pixel(x0, y0 + r, color);
  set_pixel(x0, y0 - r, color);
  set_pixel(x0 + r, y0, color);
  set_pixel(x0 - r, y0, color);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    set_pixel(x0 + x, y0 + y, color);
    set_pixel(x0 - x, y0 + y, color);
    set_pixel(x0 + x, y0 - y, color);
    set_pixel(x0 - x, y0 - y, color);
    set_pixel(x0 + y, y0 + x, color);
    set_pixel(x0 - y, y0 + x, color);
    set_pixel(x0 + y, y0 - x, color);
    set_pixel(x0 - y, y0 - x, color);
  }
}

void gfx::bitmap::draw_circle_helper(int x0, int y0, int r, int cornername,
                                     uint32_t color) {
  // Helper function for drawing circles and circular objects
  short f = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x = 0;
  short y = r;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (cornername & 0x4) {
      set_pixel(x0 + x, y0 + y, color);
      set_pixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2) {
      set_pixel(x0 + x, y0 - y, color);
      set_pixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      set_pixel(x0 - y, y0 + x, color);
      set_pixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      set_pixel(x0 - y, y0 - x, color);
      set_pixel(x0 - x, y0 - y, color);
    }
  }
}



void gfx::bitmap::fill_circle(int x0, int y0, int r, uint32_t color) {
  /* Draw a filled circle with center (x0,y0) and radius r, with given color
   * Parameters:
   *      x0: x-coordinate of center of circle. The top-left of the screen
   *          has x-coordinate 0 and increases to the right
   *      y0: y-coordinate of center of circle. The top-left of the screen
   *          has y-coordinate 0 and increases to the bottom
   *      r:  radius of circle
   *      color: 16-bit color value for the circle
   * Returns: Nothing
   */
  draw_hline(x0 - r, y0, 2 * r + 1, color);
  fill_circle_helper(x0, y0, r, 3, 0, color);
}

void gfx::bitmap::fill_circle_helper(int x0, int y0, int r, int corner,
                                     int delta, uint32_t color) {
  // Helper function for drawing filled circles
  short f = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x = 0;
  short y = r;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    if (corner & 0x1) {
      draw_hline(x0 - x, y0 - y, 2 * x + 1 + delta, color);
      draw_hline(x0 - y, y0 - x, 2 * y + 1 + delta, color);
    }
    if (corner & 0x2) {
      draw_hline(x0 - x, y0 + y, 2 * x + 1 + delta, color);
      draw_hline(x0 - y, y0 + x, 2 * y + 1 + delta, color);
    }
  }
}
