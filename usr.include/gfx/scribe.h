#pragma once

#include <gfx/bitmap.h>

namespace gfx {

  /*
   * A scibe draws on bitmaps :^)
   */
  class scribe {
    gfx::bitmap &bmp;



    struct state {
      gfx::point offset;  // for drawing at offsets
      gfx::rect clip;     // restricting draw regions
    };
    ck::vec<struct state> states;

   public:
    // enter a new state
    void enter();
    // leave the current state
    void leave();


    scribe(gfx::bitmap &b);
    ~scribe(void);

    // set a pixel in the bmp. not translated
    inline void set_pixel(int x, int y, uint32_t color) {
      bmp.set_pixel(x, y, color);
    }


    struct state &state(void) {
      return states.last();
    }

		void draw_generic_bezier(ck::vec<gfx::point> &points, uint32_t color, float stroke = 1);

    void draw_quadratic_bezier(const gfx::point &start, const gfx::point &p1,
                               const gfx::point &end, uint32_t color, float stroke = 1);

    // blend a pixel with a 0-1 alpha
    void blend_pixel(int x, int y, uint32_t color, float alpha);

    // draw a pixel, offset in the state and within the clip rect
    inline void draw_pixel(int x, int y, uint32_t color) {
      auto &s = state();
      x += s.offset.x();
      y += s.offset.y();
      if (s.clip.contains(x, y)) {
        set_pixel(x, y, color);
      }
    }

    // clear the region (clipped) with a color
    void clear(uint32_t);

    inline gfx::point translation() { return state().offset; }


    inline uint32_t width() { return bmp.width(); }
    inline uint32_t height() { return bmp.height(); }

    // draw a line between two points
    inline void draw_line(const gfx::point &p1, const gfx::point &p2,
                          uint32_t color) {
      draw_line(p1.x(), p1.y(), p2.x(), p2.y(), color);
    }


    void draw_line(int x0, int y0, int x1, int y1, uint32_t color);


    inline void draw_line_antialias(const gfx::point &p1, const gfx::point &p2,
                                    uint32_t color, float stroke = 1) {
      draw_line_antialias(p1.x(), p1.y(), p2.x(), p2.y(), color, stroke);
    }
    void draw_line_antialias(int x0, int y0, int x1, int y1, uint32_t color, float stroke = 1);
    inline void draw_vline(int x0, int y0, int h, uint32_t color) {
      draw_line(x0, y0, x0, y0 + h, color);
    }
    inline void draw_hline(int x0, int y0, int w, uint32_t color) {
      draw_line(x0, y0, x0 + w, y0, color);
    }


    // just draw a bitmap to a position (no scaling)
    void blit(const gfx::point &at, gfx::bitmap &bmp, const gfx::rect &src);


    // draw a rectangle border
    void draw_rect(const gfx::rect &r, uint32_t color);
    void fill_rect(const gfx::rect &r, uint32_t color);

    void draw_rect(const gfx::rect &r, int radius, uint32_t color);
    void fill_rect(const gfx::rect &r, int radius, uint32_t color);

    void draw_circle(int x, int y, int r, uint32_t color);
    void draw_circle_helper(int x, int y, int r, int cornername,
                            uint32_t color);


    void fill_circle(int x, int y, int r, uint32_t color);
    void fill_circle_helper(int x, int y, int r, int corner, int delta,
                            uint32_t color);
  };


};  // namespace gfx
