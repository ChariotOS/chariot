#include <chariot.h>
#include <gfx/point.h>
#include <gfx/scribe.h>
#include <math.h>
#include <stdio.h>
#include <string.h>



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
  auto clipped_rect = state().clip;
  if (clipped_rect.is_empty()) return;


  const int first_row = clipped_rect.top();
  const int last_row = clipped_rect.bottom();
  uint32_t *dst = bmp.scanline(clipped_rect.y) + clipped_rect.x;
  const size_t dst_skip = bmp.width();

  for (int row = first_row; row <= last_row; ++row) {
		for (int i = 0; i < clipped_rect.w; i++) {
			dst[i] = color;
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
  for (int row = first_row; row <= last_row; ++row) {
    memcpy(dst, src, clipped_rect.w * sizeof(uint32_t));
    dst += dst_skip;
    src += src_skip;
  }
  return;
}


void gfx::scribe::draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
  // horrizontal optimization
  if (x0 == x1) {
    auto end = min(height(), y1);
    for (int y = max(0, y0); y <= end; y++) {
      draw_pixel(x0, y, color);
    }
    return;
  }
  // vertical line optimization
  if (y0 == y1) {
    auto end = min(width(), x1);
    for (int x = max(0, x0); x <= end; x++) {
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
