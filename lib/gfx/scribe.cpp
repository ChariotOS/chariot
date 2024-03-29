#include <chariot.h>
#include <gfx/color.h>
#include <gfx/font.h>
#include <gfx/point.h>
#include <gfx/scribe.h>
#include <gfx/stackblur.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ck/time.h>
#include <ck/rand.h>

gfx::scribe::scribe(gfx::bitmap &b) : bmp(b) {
  enter();  // initial state
}


gfx::scribe::~scribe(void) { leave(); }

void gfx::scribe::enter(void) {
  if (states.size() == 0) {
    states.push({.offset = gfx::point(0, 0), .clip = gfx::rect(0, 0, width(), height())});
  } else {
    // copy the current state
    states.push(state());
  }
}

void gfx::scribe::leave() {
  if (states.size() == 0) {
    fprintf(stderr, "[gfx::scribe] attempt to leave() with no state\n");
    ::exit(-1);
  }

  states.remove(states.size() - 1);
}

// static inline int abs(int a) { return a < 0 ? -a : a; }
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }




void gfx::scribe::clear(uint32_t color) {
  fill_rect(state().clip, color);
  // auto rect = state().clip;
  // if (rect.is_empty()) return;

  // uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  // const size_t dst_skip = bmp.width();

  // for (int i = rect.h - 1; i >= 0; --i) {
  //   // fast_u32_fill(dst, color, rect.w);
  //   for (int o = 0; o < rect.w; o++) {
  //     dst[o] = color;
  //   }
  //   dst += dst_skip;
  // }
}

void gfx::scribe::blit_fast(const gfx::point &position, gfx::bitmap &source, const gfx::rect &_src_rect) {
  return;
  auto src_rect = _src_rect.intersect(source.rect());
  int dx = position.x() + translation().y();
  int dy = position.y() + translation().y();
  gfx::rect dst_rect;
  dst_rect.x = position.x() + translation().x();
  dst_rect.y = position.y() + translation().y();
  dst_rect.w = src_rect.w;
  dst_rect.h = src_rect.h;


  const int first_row = dst_rect.top();
  const int last_row = dst_rect.bottom();
  const int first_column = dst_rect.left();


  uint32_t *src = source.scanline(src_rect.y) + src_rect.x;
  uint32_t *dst = bmp.scanline(position.y()) + position.x();

  const size_t src_skip = source.width();
  const size_t dst_skip = bmp.width();

  for (int row = 0; row <= dst_rect.h; ++row) {
    char *d = (char *)dst;
    char *s = (char *)src;
    for (int i = 0; i < dst_rect.w * sizeof(uint32_t); i++) {
      d[i] = s[i];
    }
    dst += dst_skip;
    src += src_skip;
  }
}

void gfx::scribe::blit(const gfx::point &position, gfx::bitmap &source, const gfx::rect &src_rect) {
  auto safe_src_rect = src_rect.intersect(source.rect());
  // assert(source.rect().contains(safe_src_rect));

  gfx::rect dst_rect;
  dst_rect.x = position.x() + translation().x();
  dst_rect.y = position.y() + translation().y();
  dst_rect.w = safe_src_rect.w;
  dst_rect.h = safe_src_rect.h;

  auto clipped_rect = dst_rect.intersect(state().clip);
  if (clipped_rect.is_empty()) return;

  const int first_row = clipped_rect.top() - dst_rect.top();
  const int last_row = clipped_rect.bottom() - dst_rect.top();
  const int first_column = clipped_rect.left() - dst_rect.left();
  uint32_t *dst = bmp.scanline(clipped_rect.y) + clipped_rect.x;
  const size_t dst_skip = bmp.width();

  const uint32_t *src = source.scanline(src_rect.top() + first_row) + src_rect.left() + first_column;
  const size_t src_skip = source.width();

  for (int row = first_row; row <= last_row; ++row) {
    for (int i = 0; i < clipped_rect.w; i++) {
      dst[i] = (src[i] | 0xFF'00'00'00); /* Blit it with full alpha */
    }
    dst += dst_skip;
    src += src_skip;
  }
}



void gfx::scribe::blit_alpha(const gfx::point &position, gfx::bitmap &source, const gfx::rect &src_rect) {
  auto safe_src_rect = src_rect.intersect(source.rect());

  gfx::rect dst_rect;
  dst_rect.x = position.x() + translation().x();
  dst_rect.y = position.y() + translation().y();
  dst_rect.w = safe_src_rect.w;
  dst_rect.h = safe_src_rect.h;

  auto clipped_rect = dst_rect.intersect(state().clip);
  if (clipped_rect.is_empty()) return;

  const int first_row = clipped_rect.top() - dst_rect.top();
  const int last_row = clipped_rect.bottom() - dst_rect.top();
  const int first_column = clipped_rect.left() - dst_rect.left();
  uint32_t *dst = bmp.scanline(clipped_rect.y) + clipped_rect.x;
  const size_t dst_skip = bmp.width();

  const uint32_t *src = source.scanline(src_rect.top() + first_row) + src_rect.left() + first_column;
  const size_t src_skip = source.width();

  for (int row = first_row; row <= last_row; ++row) {
    for (int i = 0; i < clipped_rect.w; i++) {
      dst[i] = color::blend(src[i], dst[i] | (0xFF << 24));
    }
    dst += dst_skip;
    src += src_skip;
  }
}


void gfx::scribe::blit_scaled(gfx::bitmap &source, const gfx::rect &r, gfx::bitmap::SampleMode mode) {
  // ck::time::tracker t;
  int ox = translation().x() + r.x;
  int oy = translation().y() + r.y;
  int sw = source.width();
  int sh = source.height();


  if (mode == gfx::bitmap::SampleMode::Nearest) {
    float dfx = sw / (float)r.w;
    float dfy = sh / (float)r.h;

    int start_y = max(0, -oy);
    int end_y = min(r.h, bmp.height() - oy);
    // if (oy < 0) start_y = -oy - 1;

    float fy = dfy * start_y;

    int start_x = max(0, -ox);
    int end_x = min(r.w, bmp.width() - ox);
    // grab the scanline at the top left of the dst image
    uint32_t *dst = bmp.scanline(start_y + oy) + ox;

    for (int y = start_y; y < end_y; y++) {
      // grab the scanline that we sample from
      uint32_t *src = source.scanline(fy);

      float fx = start_x * dfx;
      for (int x = start_x; x < end_x; x++) {
        dst[x] = src[(int)fx];
        fx += dfx;
      }
      fy += dfy;
      // go to the next scanline on the dst image
      dst += bmp.width();
    }


  } else {
    for (int y = 0; y < r.h; y++) {
      for (int x = 0; x < r.w; x++) {
        float fx = x / (float)r.w;
        float fy = y / (float)r.h;
        set_pixel(x + ox, y + oy, source.sample(fx, fy, mode));
      }
    }
  }

  // printf("blit_scaled (%d, %d - %d, %d) took %llu us\n", ox, oy, r.w, r.h, t.us());
}

void gfx::scribe::blend_pixel(int x, int y, uint32_t color, float alpha) {
  auto &s = state();
  // x += s.offset.x();
  // y += s.offset.y();
  // if (!s.clip.contains(x, y)) return;


  uint32_t fgi = (color & 0xFF'FF'FF) | ((int)(255 * alpha) << 24);
  uint32_t bgi = bmp.get_pixel(x + s.offset.x(), y + s.offset.y());
  draw_pixel(x, y, gfx::color::blend(fgi, bgi));
}

#define __ipart(X) ((int)(X))
#define __round(X) ((int)(((double)(X)) + 0.5))
#define __fpart(X) (((double)(X)) - (double)__ipart(X))
#define __rfpart(X) (1.0 - __fpart(X))
#define __plot(__x, __y, __brightness) blend_pixel((__x), (__y), color, __brightness)

#define __swap(__x, __y)         \
  ({                             \
    __typeof__(__x) __tmp = __x; \
    __x = __y;                   \
    __y = __tmp;                 \
  })

void gfx::scribe::draw_line_antialias(int x0i, int y0i, int x1i, int y1i, uint32_t color, float wd) {
  double x0 = x0i;
  double y0 = y0i;
  double x1 = x1i;
  double y1 = y1i;


#if 0
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx - dy, e2, x2, y2; /* error value e_xy */
  float ed = dx + dy == 0 ? 1 : sqrt((float)dx * dx + (float)dy * dy);

  for (wd = (wd + 1) / 2;;) { /* pixel loop */
    // blend_pixel(x0, y0, color, fmax(0, (fabs(err - dx + dy) / ed - wd + 1)));
		draw_pixel(x0, y0, color);
    e2 = err;
    x2 = x0;
    if (2 * e2 >= -dx) { /* x step */
      for (e2 += dy, y2 = y0; e2 < ed * wd && (y1 != y2 || dx > dy); e2 += dx) {
        // blend_pixel(x0, y2 += sy, color, fmax(0, (fabs(e2) / ed - wd + 1)));
				draw_pixel(x0, y2 + sy, color);
			}
      if (x0 == x1) break;
      e2 = err;
      err -= dy;
      x0 += sx;
    }
    if (2 * e2 <= dy) { /* y step */
      for (e2 = dx - e2; e2 < ed * wd && (x1 != x2 || dx < dy); e2 += dy) {
        // blend_pixel(x2 += sx, y0, color, fmax(0, (fabs(e2) / ed - wd + 1)));
				draw_pixel(x2 += sx, y0, color);
			}
      if (y0 == y1) break;
      err += dx;
      y0 += sy;
    }
  }


#else

  const bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    __swap(x0, y0);
    __swap(x1, y1);
  }
  if (x0 > x1) {
    __swap(x0, x1);
    __swap(y0, y1);
  }

  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float gradient = (dx == 0) ? 1 : dy / dx;

  int xpx11;
  float intery;
  {
    const float xend = __round(x0);
    const float yend = y0 + gradient * (xend - x0);
    const float xgap = __rfpart(x0 + 0.5);
    xpx11 = (int)xend;
    const int ypx11 = __ipart(yend);
    if (steep) {
      __plot(ypx11, xpx11, __rfpart(yend) * xgap);
      __plot(ypx11 + 1, xpx11, __fpart(yend) * xgap);
    } else {
      __plot(xpx11, ypx11, __rfpart(yend) * xgap);
      __plot(xpx11, ypx11 + 1, __fpart(yend) * xgap);
    }
    intery = yend + gradient;
  }

  int xpx12;
  {
    const float xend = __round(x1);
    const float yend = y1 + gradient * (xend - x1);
    const float xgap = __rfpart(x1 + 0.5);
    xpx12 = (int)xend;
    const int ypx12 = __ipart(yend);
    if (steep) {
      __plot(ypx12, xpx12, __rfpart(yend) * xgap);
      __plot(ypx12 + 1, xpx12, __fpart(yend) * xgap);
    } else {
      __plot(xpx12, ypx12, __rfpart(yend) * xgap);
      __plot(xpx12, ypx12 + 1, __fpart(yend) * xgap);
    }
  }

  if (steep) {
    for (int x = xpx11 + 1; x < xpx12; x++) {
      __plot(__ipart(intery), x, __rfpart(intery));
      __plot(__ipart(intery) + 1, x, __fpart(intery));
      intery += gradient;
    }
  } else {
    for (int x = xpx11 + 1; x < xpx12; x++) {
      __plot(x, __ipart(intery), __rfpart(intery));
      __plot(x, __ipart(intery) + 1, __fpart(intery));
      intery += gradient;
    }
  }
#endif
}


static double getPt(double n1, double n2, float perc) {
  double diff = n2 - n1;

  return n1 + (diff * perc);
}

void gfx::scribe::draw_generic_bezier(ck::vec<gfx::point> &points, uint32_t color, float stroke) {
  /*
for (int i = 0; i < points.size() - 1; i++) {
auto &p1 = points[i];
auto &p2 = points[i + 1];
draw_line_antialias(p1, p2, color, stroke);
}
  */

  int lx = points[0].x();
  int ly = points[0].y();

  ck::vec<gfx::float2> tmp;
  for (auto &p : points)
    tmp.push(gfx::float2(p.x(), p.y()));

  for (float t = 0; t < 1; t += 0.001) {
    // copy, idk.
    for (int i = 0; i < points.size(); i++)
      tmp[i] = points[i];

    int i = points.size() - 1;
    while (i > 0) {
      for (int k = 0; k < i; k++)
        tmp[k] = tmp[k] + ((tmp[k + 1] - tmp[k]) * t);
      i--;
    }

    double x = tmp[0].x();
    double y = tmp[0].y();

    draw_line_antialias(gfx::point(lx, ly), gfx::point(x, y), color, stroke);

    lx = x;
    ly = y;
  }
}



void gfx::scribe::draw_quadratic_bezier(
    const gfx::point &start, const gfx::point &p1, const gfx::point &end, uint32_t color, float stroke) {
  int lx = start.x();
  int ly = start.y();


  for (float i = 0; i < 1; i += 0.001) {
    // The Green Line
    double xa = getPt(start.x(), p1.x(), i);
    double ya = getPt(start.y(), p1.y(), i);
    double xb = getPt(p1.x(), end.x(), i);
    double yb = getPt(p1.y(), end.y(), i);

    // The Black Dot
    int x = getPt(xa, xb, i);
    int y = getPt(ya, yb, i);

    draw_line_antialias(gfx::point(lx, ly), gfx::point(x, y), color, stroke);

    lx = x;
    ly = y;
  }
}




void gfx::scribe::draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
  // horrizontal optimization
  if (x0 == x1) {
    auto end = min(height(), max(y0, y1));
    for (int y = max(0, min(y0, y1)); y < end; y++) {
      draw_pixel(x0, y, color);
    }
    return;
  }
  // vertical line optimization
  if (y0 == y1) {
    auto end = min(width(), max(x0, x1));
    for (int x = max(0, min(x0, x1)); x < end; x++) {
      draw_pixel(x, y0, color);
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
      draw_pixel(y0, x0, color);
    } else {
      draw_pixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}


void gfx::scribe::draw_rect_special(const gfx::rect &r, uint32_t top_left, uint32_t bottom_right) {
  // bottom
  draw_hline(r.left(), r.bottom(), r.w, bottom_right);
  // right
  draw_vline(r.right(), r.top(), r.h, bottom_right);

  // top
  draw_hline(r.left(), r.top(), r.w, top_left);
  // left
  draw_vline(r.left(), r.top(), r.h, top_left);
}

void gfx::scribe::draw_rect(const gfx::rect &r, uint32_t color) { return draw_rect_special(r, color, color); }

void gfx::scribe::fill_rect(const gfx::rect &d, uint32_t color) {
  auto rect = gfx::rect(d.x + translation().x(), d.y + translation().y(), d.w, d.h).intersect(bmp.rect()).intersect(state().clip);
  if (rect.is_empty()) return;


  uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  const size_t dst_skip = bmp.width();

  for (int i = rect.h - 1; i >= 0; --i) {
    for (int j = 0; j < rect.w; ++j)
      dst[j] = color;
    dst += dst_skip;
  }
}



void gfx::scribe::fill_rect_alpha(const gfx::rect &d, uint32_t color, float alpha) {
  auto rect = gfx::rect(d.x + translation().x(), d.y + translation().y(), d.w, d.h).intersect(bmp.rect()).intersect(state().clip);
  if (rect.is_empty()) return;


  color = gfx::color::alpha(color, alpha);

  uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  const size_t dst_skip = bmp.width();

  for (int i = rect.h - 1; i >= 0; --i) {
    for (int j = 0; j < rect.w; ++j)
      dst[j] = gfx::color::blend(color, dst[j] | 0xFF'000000);
    dst += dst_skip;
  }
}


void gfx::scribe::fill_rect_radius(const gfx::rect &rect, int r, uint32_t color) {
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


void gfx::scribe::draw_rect_radius(const gfx::rect &rect, int r, uint32_t color) {
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

void gfx::scribe::draw_circle(int x0, int y0, int r, uint32_t color) {
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

  draw_pixel(x0, y0 + r, color);
  draw_pixel(x0, y0 - r, color);
  draw_pixel(x0 + r, y0, color);
  draw_pixel(x0 - r, y0, color);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    draw_pixel(x0 + x, y0 + y, color);
    draw_pixel(x0 - x, y0 + y, color);
    draw_pixel(x0 + x, y0 - y, color);
    draw_pixel(x0 - x, y0 - y, color);
    draw_pixel(x0 + y, y0 + x, color);
    draw_pixel(x0 - y, y0 + x, color);
    draw_pixel(x0 + y, y0 - x, color);
    draw_pixel(x0 - y, y0 - x, color);
  }
}

void gfx::scribe::draw_circle_helper(int x0, int y0, int r, int cornername, uint32_t color) {
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
      draw_pixel(x0 + x, y0 + y, color);
      draw_pixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2) {
      draw_pixel(x0 + x, y0 - y, color);
      draw_pixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      draw_pixel(x0 - y, y0 + x, color);
      draw_pixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      draw_pixel(x0 - y, y0 - x, color);
      draw_pixel(x0 - x, y0 - y, color);
    }
  }
}



void gfx::scribe::fill_circle(int x0, int y0, int r, uint32_t color) {
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

void gfx::scribe::fill_circle_helper(int x0, int y0, int r, int corner, int delta, uint32_t color) {
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


void gfx::scribe::draw_frame(const gfx::rect &frame, uint32_t bg, uint32_t fg) {
  auto highlight = gfx::color::blend(gfx::color::alpha(0xFFFFFF, 0.8), bg);
  auto shadow = gfx::color::blend(gfx::color::alpha(fg, 0.2), bg);

  auto l = frame.left();
  auto t = frame.top();

  for (int y = 0; y < frame.h; y++) {
    draw_pixel(0 + l, y + t, fg);
    draw_pixel(frame.w + l - 1, y + t, fg);

    // top or bottom black bar
    if (unlikely(y == 0 || y == frame.h - 1)) {
      for (int x = 0; x < frame.w; x++)
        draw_pixel(x + l, y + t, fg);
      continue;
    }

    // top highlight
    if (unlikely(y == 1)) {
      for (int x = 1; x < frame.w - 1; x++)
        draw_pixel(x + l, y + t, highlight);
      continue;
    }

    // bottom shadow
    if (unlikely(y == frame.h - 2)) {
      draw_pixel(1 + l, y + t, highlight);
      for (int x = 2; x < frame.w - 1; x++)
        draw_pixel(x + l, y + t, shadow);
      continue;
    }

    draw_pixel(1 + l, y + t, highlight);
    draw_pixel(l + frame.w - 2, y + t, shadow);


    for (int x = 2; x < frame.w - 1; x++) {
      draw_pixel(x + l, y + t, bg);
    }
  }
}

void gfx::scribe::draw_text(
    gfx::font &font, const gfx::rect &rect, const ck::string &text, ui::TextAlign align, uint32_t color, bool elide) {
  auto lines = text.split('\n', true);

  static const int line_spacing = 0;
  int line_height = font.line_height() + line_spacing;
  gfx::rect bounding_rect{0, 0, 0, (static_cast<int>(lines.size()) * line_height) - line_spacing};

  for (auto &line : lines) {
    auto line_width = font.width(line);
    if (line_width > bounding_rect.w) bounding_rect.w = line_width;
  }

  bounding_rect.w = min(bounding_rect.w, rect.w);

  switch (align) {
    case ui::TextAlign::TopLeft:
      bounding_rect.set_location(rect.x, rect.y);
      break;
    case ui::TextAlign::TopRight:
      bounding_rect.set_location((rect.right() + 1) - bounding_rect.w, rect.y);
      break;
    case ui::TextAlign::CenterLeft:
      bounding_rect.x = rect.x;
      bounding_rect.center_vertically_within(rect);
      break;
    case ui::TextAlign::CenterRight:
      bounding_rect.x = (rect.right() + 1) - bounding_rect.w;
      bounding_rect.center_vertically_within(rect);
      break;
    case ui::TextAlign::Center:
      bounding_rect.center_within(rect);
      break;
    case ui::TextAlign::BottomRight:
      bounding_rect.set_location((rect.right() + 1) - bounding_rect.w, (rect.bottom() + 1) - bounding_rect.h);
      break;
    case ui::TextAlign::BottomLeft:
      bounding_rect.set_location(rect.x, (rect.bottom() + 1) - bounding_rect.h);
      break;
    case ui::TextAlign::BottomCenter:
      bounding_rect.y = (rect.bottom() + 1) - bounding_rect.h;
      bounding_rect.center_horizontally_within(rect);
      break;
    case ui::TextAlign::TopCenter:
      bounding_rect.y = rect.y;
      bounding_rect.center_horizontally_within(rect);
      break;
    default:
      panic("INVALID TEXT ALIGNMENT %d\n", align);
  }




  for (size_t i = 0; i < lines.size(); ++i) {
    auto &line = lines[i];
    gfx::rect line_rect(bounding_rect.x, bounding_rect.y + static_cast<int>(i) * line_height, bounding_rect.w, line_height);
    line_rect.intersect(rect);
    draw_text_line(font, line_rect, line, align, color, elide);
  }
}


void gfx::scribe::stackblur(int radius, const gfx::rect &area) {
  gfx::rect a = area.shifted(translation().x(), translation().y());
  a.w += 1;
  a.h += 1;
  gfx::stackblur(bmp.pixels(), bmp.width(), bmp.height(), radius, a);
}

void gfx::scribe::noise(float opacity, const gfx::rect &area) {
  auto rect =
      gfx::rect(area.x + translation().x(), area.y + translation().y(), area.w, area.h).intersect(bmp.rect()).intersect(state().clip);
  if (rect.is_empty()) return;
  ck::rand rng(0xDEADBEEF | ((rect.x << 16) & 0xFFFF) | (rect.y & 0xFFFF));


  uint32_t alpha_mask = ((uint32_t)(255 * opacity) << 24);
  uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  const size_t dst_skip = bmp.width();

  for (int i = rect.h - 1; i >= 0; --i) {
    for (int j = 0; j < rect.w; ++j) {
      float noise = rng.next() & 0xFF;

      uint32_t fgi = (gfx::color::rgb(noise, noise, noise) & 0xFF'FF'FF) | alpha_mask;
      dst[j] = gfx::color::blend(fgi, dst[j]);
    }
    dst += dst_skip;
  }
}

void gfx::scribe::saturation(float value, const gfx::rect &area) {
  auto rect =
      gfx::rect(area.x + translation().x(), area.y + translation().y(), area.w, area.h).intersect(bmp.rect()).intersect(state().clip);
  if (rect.is_empty()) return;
  ck::rand rng(0xDEADBEEF | ((rect.x << 16) & 0xFFFF) | (rect.y & 0xFFFF));


  uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  const size_t dst_skip = bmp.width();

  for (int i = rect.h - 1; i >= 0; --i) {
    for (int j = 0; j < rect.w; ++j) {
      gfx::color::impl::color color = dst[j];

      // weights from CCIR 601 spec
      // https://stackoverflow.com/questions/13806483/increase-or-decrease-color-saturation
      double gray = 0.2989 * color.red() + 0.5870 * color.green() + 0.1140 * color.blue();

      uint8_t red = (uint8_t)CLAMP(-gray * value + color.red() * (1 + value), 0, 255);
      uint8_t green = (uint8_t)CLAMP(-gray * value + color.green() * (1 + value), 0, 255);
      uint8_t blue = (uint8_t)CLAMP(-gray * value + color.blue() * (1 + value), 0, 255);

      dst[j] = gfx::color::rgb(red, green, blue);
    }
    dst += dst_skip;
  }
}


void gfx::scribe::sepia(float value, const gfx::rect &area) {
  auto rect =
      gfx::rect(area.x + translation().x(), area.y + translation().y(), area.w, area.h).intersect(bmp.rect()).intersect(state().clip);
  if (rect.is_empty()) return;
  ck::rand rng(0xDEADBEEF | ((rect.x << 16) & 0xFFFF) | (rect.y & 0xFFFF));


  uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  const size_t dst_skip = bmp.width();
  uint32_t alpha_mask = ((uint32_t)(255 * value) << 24);

  for (int i = rect.h - 1; i >= 0; --i) {
    for (int j = 0; j < rect.w; ++j) {
      gfx::color::impl::color color = dst[j];

      uint32_t red = (color.red() * 0.393) + (color.green() * 0.769) + (color.blue() * 0.189);
      uint32_t green = (color.red() * 0.349) + (color.green() * 0.686) + (color.blue() * 0.168);
      uint32_t blue = (color.red() * 0.272) + (color.green() * 0.534) + (color.blue() * 0.131);

      uint32_t sepia_color = gfx::color::rgb(MIN(255, red), MIN(255, green), MIN(blue, 255));

      dst[j] = gfx::color::blend(color.value | 0xFF'000000, sepia_color | alpha_mask);
    }
    dst += dst_skip;
  }
}


void gfx::scribe::draw_text_line(
    gfx::font &font, const gfx::rect &a_rect, const ck::string &text, ui::TextAlign align, uint32_t color, bool elide) {
  auto rect = a_rect;
  ck::string final_text = text;


  if (elide) {
    int text_width = font.width(final_text);
    if (text_width > rect.w) {
      int glyph_spacing = 0;  // TODO: font.glyph_spacing();
      int byte_offset = 0;
      int new_width = font.width("...");
      if (new_width < text_width) {
        for (auto i = 0; i < final_text.len(); i++) {
          uint32_t code_point = final_text[i];
          int glyph_width = font.width(code_point);
          // NOTE: Glyph spacing should not be added after the last glyph on the line,
          //       but since we are here because the last glyph does not actually fit on the line,
          //       we don't have to worry about spacing.
          int width_with_this_glyph_included = new_width + glyph_width + glyph_spacing;
          if (width_with_this_glyph_included > rect.w) break;
          byte_offset = i;
          new_width += glyph_width + glyph_spacing;
        }

        auto sub = text.substring_view(0, byte_offset);
        ck::string builder;
        for (int i = 0; i < sub.len(); i++)
          builder.push(sub[i]);
        builder += "...";
        final_text.clear();
        final_text = builder;
      }
    }
  }



  switch (align) {
    case ui::TextAlign::TopLeft:
    case ui::TextAlign::CenterLeft:
      break;
    case ui::TextAlign::TopRight:
    case ui::TextAlign::CenterRight:
    case ui::TextAlign::BottomRight:
      rect.x = rect.right() - font.width(final_text);
      break;
    case ui::TextAlign::Center: {
      auto shrunken_rect = rect;
      shrunken_rect.w = font.width(final_text);
      shrunken_rect.center_within(rect);
      rect = shrunken_rect;
      break;
    }
    default:
      break;
      // panic("INVALID TEXT ALIGNMENT\n");
  }


  // draw_rect(rect, 0xFF00FF);
  auto p = gfx::printer(*this, font, rect.x, rect.y + font.ascent(), rect.w);
  p.set_color(color);
  p.write_utf8(final_text.get());
}


void gfx::scribe::draw_triangle(const gfx::point &a, const gfx::point &b, const gfx::point &c, uint32_t rgba) {
  int ox = state().offset.x();
  int oy = state().offset.y();

  gfx::point p0(a.translated(ox, oy));
  gfx::point p1(b.translated(ox, oy));
  gfx::point p2(c.translated(ox, oy));

  // sort points from top to bottom
  if (p0.y() > p1.y()) swap(p0, p1);
  if (p0.y() > p2.y()) swap(p0, p2);
  if (p1.y() > p2.y()) swap(p1, p2);

  // return if top and bottom points are on same line
  if (p0.y() == p2.y()) return;

  // return if top is below clip rect or bottom is above clip rect
  auto clip = clip_rect();
  if (p0.y() >= clip.bottom()) return;
  if (p2.y() < clip.top()) return;

  float dx02 = (float)(p2.x() - p0.x()) / (p2.y() - p0.y());
  float x01 = p0.x();
  float x02 = p0.x();

  if (p0.y() != p1.y()) {  // p0 and p1 are on different lines
    float dx01 = (float)(p1.x() - p0.x()) / (p1.y() - p0.y());

    int top = p0.y();
    if (top < clip.top()) {
      x01 += dx01 * (clip.top() - top);
      x02 += dx02 * (clip.top() - top);
      top = clip.top();
    }

    for (int y = top; y < p1.y() && y < clip.bottom(); ++y) {  // XXX <=?
      int start = x01 > x02 ? max((int)x02, clip.left()) : max((int)x01, clip.left());
      int end = x01 > x02 ? min((int)x01, clip.right()) : min((int)x02, clip.right());
      auto *scanline = bmp.scanline(y);
      for (int x = start; x < end; x++) {
        scanline[x] = rgba;
      }
      x01 += dx01;
      x02 += dx02;
    }
  }

  // return if middle point and bottom point are on same line
  if (p1.y() == p2.y()) return;

  float x12 = p1.x();
  float dx12 = (float)(p2.x() - p1.x()) / (p2.y() - p1.y());
  int top = p1.y();
  if (top < clip.top()) {
    x02 += dx02 * (clip.top() - top);
    x12 += dx12 * (clip.top() - top);
    top = clip.top();
  }

  for (int y = top; y < p2.y() && y < clip.bottom(); ++y) {  // XXX <=?
    int start = x12 > x02 ? max((int)x02, clip.left()) : max((int)x12, clip.left());
    int end = x12 > x02 ? min((int)x12, clip.right()) : min((int)x02, clip.right());
    auto *scanline = bmp.scanline(y);
    for (int x = start; x < end; x++) {
      scanline[x] = rgba;
    }
    x02 += dx02;
    x12 += dx12;
  }
}



static void scribe_draw_text_callback(char c, void *arg) {
  auto *f = (gfx::printer *)arg;
  f->write(c);
}


extern "C" int vfctprintf(void (*out)(char character, void *arg), void *arg, const char *format, va_list va);

void gfx::printer::printf(const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vfctprintf(scribe_draw_text_callback, (void *)this, fmt, va);
  va_end(va);
}



void gfx::printer::write(uint32_t c) {
  int x = pos.x();
  int y = pos.y();
  uint32_t right_edge = this->x0 + width;

  fnt->draw(x, y, this->sc, c, color);

  /* flush the position change */
  pos.set_x(x);
  pos.set_y(y);
}




void gfx::printer::write(const char *str) {
  for (int i = 0; str[i] != 0; i++) {
    write(str[i]);
    // pos.set_x(pos.x() + fnt->kerning_for(str[i], str[i+1]));
  }
}


/*
 * utf8_to_unicode()
 *
 * Convert a UTF-8 sequence to its unicode value, and return the length of
 * the sequence in bytes.
 *
 * NOTE! Invalid UTF-8 will be converted to a one-byte sequence, so you can
 * either use it as-is (ie as Latin1) or you can check for invalid UTF-8
 * by checking for a length of 1 and a result > 127.
 *
 * NOTE 2! This does *not* verify things like minimality. So overlong forms
 * are happily accepted and decoded, as are the various "invalid values".
 */
static unsigned utf8_to_unicode(char *line, unsigned index, unsigned len, uint32_t *res) {
  unsigned value;
  unsigned char c = line[index];
  unsigned bytes, mask, i;

  *res = c;
  line += index;
  len -= index;

  /*
   * 0xxxxxxx is valid utf8
   * 10xxxxxx is invalid UTF-8, we assume it is Latin1
   */
  if (c < 0xc0) return 1;

  /* Ok, it's 11xxxxxx, do a stupid decode */
  mask = 0x20;
  bytes = 2;
  while (c & mask) {
    bytes++;
    mask >>= 1;
  }

  /* Invalid? Do it as a single byte Latin1 */
  if (bytes > 6) return 1;
  if (bytes > len) return 1;

  value = c & (mask - 1);

  /* Ok, do the bytes */
  for (i = 1; i < bytes; i++) {
    c = line[i];
    if ((c & 0xc0) != 0x80) return 1;
    value = (value << 6) | (c & 0x3f);
  }
  *res = value;
  return bytes;
}

void gfx::printer::write_utf8(const char *line) {
  int x = pos.x();
  int y = pos.y();
  uint32_t right_edge = this->x0 + width;


  int len = strlen(line);
  // ::printf("len = %d\n", len);
  // ::printf("line = '%s'\n", line);
  // ck::hexdump((void *)line, len);
  for (auto ind = 0; ind < len;) {
    uint32_t cp = '?';
    auto step = utf8_to_unicode((char *)line, ind, len, &cp);
    // ::printf("%04x, %d -> %d\n", cp, ind, ind + step);
    ind += step;
    fnt->draw(x, y, this->sc, cp, color);
  }
  // ::printf("\n");


  /* flush the position change */
  pos.set_x(x);
  pos.set_y(y);
}
