#include <chariot.h>
#include <gfx/color.h>
#include <gfx/font.h>
#include <gfx/point.h>
#include <gfx/scribe.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


gfx::scribe::scribe(gfx::bitmap &b) : bmp(b) {
  enter();  // initial state
}


gfx::scribe::~scribe(void) { leave(); }

void gfx::scribe::enter(void) {
  if (states.size() == 0) {
    states.push({.offset = gfx::point(0, 0),
                 .clip = gfx::rect(0, 0, width(), height())});
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
  auto rect = state().clip;
  if (rect.is_empty()) return;

  uint32_t *dst = bmp.scanline(rect.top()) + rect.left();
  const size_t dst_skip = bmp.width();

  for (int i = rect.h - 1; i >= 0; --i) {
    // fast_u32_fill(dst, color, rect.w);
    for (int o = 0; o < rect.w; o++) {
      dst[o] = color;
    }
    dst += dst_skip;
  }
}

void gfx::scribe::blit(const gfx::point &position, gfx::bitmap &source,
                       const gfx::rect &src_rect) {
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

  const uint32_t *src = source.scanline(src_rect.top() + first_row) +
                        src_rect.left() + first_column;
  const size_t src_skip = source.width();


  // ::printf("w: %4d,  last_row: %4d\n", last_row, clipped_rect.w-1);
  for (int row = first_row; row < last_row; ++row) {
    memcpy(dst, src, clipped_rect.w * sizeof(uint32_t));
    dst += dst_skip;
    src += src_skip;
  }

  // draw_pixel(dst_rect.x, dst_rect.y, 0xFF00FF);
  return;
}


void gfx::scribe::blend_pixel(int x, int y, uint32_t color, float alpha) {
  auto &s = state();
  x += s.offset.x();
  y += s.offset.y();
  if (!s.clip.contains(x, y)) return;


  uint32_t fgi = (color & 0xFF'FF'FF) | ((int)(255 * alpha) << 24);
  uint32_t bgi = bmp.get_pixel(
      x, y);  // this could be slow, cause we read from vga memory...
  set_pixel(x, y, gfx::color::blend(fgi, bgi));
}

#define __ipart(X) ((int)(X))
#define __round(X) ((int)(((double)(X)) + 0.5))
#define __fpart(X) (((double)(X)) - (double)__ipart(X))
#define __rfpart(X) (1.0 - __fpart(X))
#define __plot(__x, __y, __brightness) \
  blend_pixel((__x), (__y), color, __brightness)

#define __swap(__x, __y)         \
  ({                             \
    __typeof__(__x) __tmp = __x; \
    __x = __y;                   \
    __y = __tmp;                 \
  })

void gfx::scribe::draw_line_antialias(int x0i, int y0i, int x1i, int y1i,
                                      uint32_t color, float wd) {
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

void gfx::scribe::draw_generic_bezier(ck::vec<gfx::point> &points,
                                      uint32_t color, float stroke) {
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
  for (auto &p : points) tmp.push(gfx::float2(p.x(), p.y()));

  for (float t = 0; t < 1; t += 0.001) {
    // copy, idk.
    for (int i = 0; i < points.size(); i++) tmp[i] = points[i];

    int i = points.size() - 1;
    while (i > 0) {
      for (int k = 0; k < i; k++) tmp[k] = tmp[k] + ((tmp[k + 1] - tmp[k]) * t);
      i--;
    }

    double x = tmp[0].x();
    double y = tmp[0].y();

    draw_line_antialias(gfx::point(lx, ly), gfx::point(x, y), color, stroke);

    lx = x;
    ly = y;
  }
}



void gfx::scribe::draw_quadratic_bezier(const gfx::point &start,
                                        const gfx::point &p1,
                                        const gfx::point &end, uint32_t color,
                                        float stroke) {
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
    for (int y = max(0, min(y0, y1)); y <= end; y++) {
      draw_pixel(x0, y, color);
    }
    return;
  }
  // vertical line optimization
  if (y0 == y1) {
    auto end = min(width(), max(x0, x1));
    for (int x = max(0, min(x0, x1)); x <= end; x++) {
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




void gfx::scribe::draw_rect(const gfx::rect &r, uint32_t color) {
  // top
  draw_line(r.left(), r.top(), r.right(), r.top(), color);

  // left
  draw_line(r.left(), r.top(), r.left(), r.bottom(), color);
  // right
  draw_line(r.right(), r.top(), r.right(), r.bottom(), color);

  // bottom
  draw_line(r.left(), r.bottom(), r.right(), r.bottom(), color);
}

void gfx::scribe::fill_rect(const gfx::rect &d, uint32_t color) {
  auto r = gfx::rect(0, 0, width(), height()).intersect(d);
  for (int y = r.top(); y < r.bottom(); y++) {
    for (int x = r.left(); x < r.right(); x++) {
      draw_pixel(x, y, color);
    }
  }
}



void gfx::scribe::draw_rect(const gfx::rect &rect, int r, uint32_t color) {
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


void gfx::scribe::fill_rect(const gfx::rect &rect, int r, uint32_t color) {
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

void gfx::scribe::draw_circle_helper(int x0, int y0, int r, int cornername,
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

void gfx::scribe::fill_circle_helper(int x0, int y0, int r, int corner,
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


void gfx::scribe::draw_frame(const gfx::rect &frame, uint32_t bg) {
  auto highlight = gfx::color::blend(gfx::color::alpha(0xFFFFFF, 0.8), bg);
  auto shadow = gfx::color::blend(gfx::color::alpha(0x000000, 0.2), bg);

  auto l = frame.left();
  auto t = frame.top();

  for (int y = 0; y < frame.h; y++) {
    draw_pixel(0 + l, y + t, 0x000000);
    draw_pixel(frame.w + l - 1, y + t, 0x000000);

    // top or bottom black bar
    if (unlikely(y == 0 || y == frame.h - 1)) {
      for (int x = 0; x < frame.w; x++) draw_pixel(x + l, y + t, 0x000000);
      continue;
    }

    // top highlight
    if (unlikely(y == 1)) {
      for (int x = 1; x < frame.w - 1; x++) draw_pixel(x + l, y + t, highlight);
      continue;
    }

    // bottom shadow
    if (unlikely(y == frame.h - 2)) {
      draw_pixel(1 + l, y + t, highlight);
      for (int x = 2; x < frame.w - 1; x++) draw_pixel(x + l, y + t, shadow);
      continue;
    }

    draw_pixel(1 + l, y + t, highlight);
    draw_pixel(l + frame.w - 2, y + t, shadow);


    for (int x = 2; x < frame.w - 1; x++) {
      draw_pixel(x + l, y + t, bg);
    }
  }
}

#if 0
void gfx::scribe::draw_text(struct text_thunk &st, gfx::font &fnt, char c,
                            uint32_t color, int flags) {
  int x = st.pos.x();
  int y = st.pos.y();
  uint32_t right_edge = st.x0 + st.width;

  // if (getpid() != 6)::printf("'%c' %d %d\n", c, x, y);

  auto newline = [&] {
    x = st.x0;
    // int oy = y;
    y += fnt.line_height();
    // if (getpid() != 6) ::printf("y: %4d -> %-4d   %d\n", oy, y,
    // fnt.line_height());
  };

  if (c == '\n') {
    newline();
  } else {
    if (x + fnt.width(c) > right_edge) {
      newline();
    }

    int dy = y + fnt.line_height();
    fnt.draw(x, dy, *this, c, color);
  }
  st.pos.set_x(x);
  st.pos.set_y(y);
}
#endif




static void scribe_draw_text_callback(char c, void *arg) {
  auto *f = (gfx::printer *)arg;
  f->write(c);
}


extern "C" int vfctprintf(void (*out)(char character, void *arg), void *arg,
                          const char *format, va_list va);

void gfx::printer::printf(const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vfctprintf(scribe_draw_text_callback, (void *)this, fmt, va);
  va_end(va);
}


void gfx::printer::write(char c) {
  int x = pos.x();
  int y = pos.y();
  uint32_t right_edge = this->x0 + width;


  auto newline = [&] {
    x = this->x0;
    // int oy = y;
    y += fnt->line_height();
    // if (getpid() != 6) ::printf("y: %4d -> %-4d   %d\n", oy, y,
    // fnt.line_height());
  };

  if (c == '\n') {
    newline();
  } else {
    if (x + fnt->width(c) > right_edge) {
      newline();
    }

    int dy = y + fnt->line_height();
    fnt->draw(x, dy, this->sc, c, color);
  }

	/* flush the position change */
  pos.set_x(x);
  pos.set_y(y);
}


void gfx::printer::write(const char *str) {
  for (int i = 0; str[i] != 0; i++) {
    write(str[i]);
  }
}

