#pragma once

#include <gfx/bitmap.h>
#include <ui/textalign.h>

namespace gfx {

  // fwd decl
  class font;
  class scribe;



  // allows you to continue drawing text from the last position
  class printer {
   public:
    inline printer(gfx::scribe &sc, gfx::font &font, int x, int y, int width)
        : sc(sc) {
      set_font(font);
      x0 = x;
      y0 = y;
      this->width = width;
      pos.set_x(x);
      pos.set_y(y);
    }


    void printf(const char *fmt, ...);
    void write(char c);
    void write(const char *msg);

    auto set_color(uint32_t c) { color = c; }
    auto get_color(void) { return color; }

    /*
     * The font management is a little goofy, It requires that the owner
     * maintains a reference to the original font, as we don't take in a
     * ck::ref<gfx::font> here...
     */
    void set_font(gfx::font &fnt) { this->fnt = &fnt; }

    auto get_pos() { return pos; }



   protected:
    uint32_t color;

    gfx::scribe &sc;
    gfx::font *fnt;
    int x0, y0, width;

    gfx::point pos;

    friend class scribe;
  };

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

    friend class printer;

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

    void draw_generic_bezier(ck::vec<gfx::point> &points, uint32_t color,
                             float stroke = 1);

    void draw_quadratic_bezier(const gfx::point &start, const gfx::point &p1,
                               const gfx::point &end, uint32_t color,
                               float stroke = 1);

    // blend a pixel with a 0-1 alpha
    void blend_pixel(int x, int y, uint32_t color, float alpha);

    // draw a pixel, offset in the state and within the clip rect
    inline void draw_pixel(int x, int y, uint32_t color) {
      auto &s = state();
      x += s.offset.x();
      y += s.offset.y();

      auto &c = s.clip;

      if (x >= c.left() && x <= c.right() && y >= c.top() && y <= c.bottom()) {
        set_pixel(x, y, color);
      }
    }

		void draw_text(gfx::font &fnt, const gfx::rect &bounds, const ck::string &text, ui::TextAlign, uint32_t color, bool elide = true);
		void draw_text_line(gfx::font &fnt, const gfx::rect &bounds, const ck::string &text, ui::TextAlign, uint32_t color, bool elide = true);


    // draw text, wrapping within
    /*
void draw_text(struct printer &st, gfx::font &fnt, const char *str,
       uint32_t color, int flags = 0);

// with a printf thing
void printf(struct printer &st, gfx::font &fnt, uint32_t color, int flags,
    const char *fmt, ...);

void draw_text(struct printer &st, gfx::font &fnt, char c, uint32_t color,
       int flags = 0);


inline void draw_text(const char *str, gfx::font &fnt, int width,
              gfx::point &pos, uint32_t color, int flags = 0) {
auto thnk = printer(pos.x(), pos.y(), width);
draw_text(thnk, fnt, str, color, flags);
}
    */

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
    void draw_line_antialias(int x0, int y0, int x1, int y1, uint32_t color,
                             float stroke = 1);
    inline void draw_vline(int x0, int y0, int h, uint32_t color) {
      draw_line(x0, y0, x0, y0 + h, color);
    }
    inline void draw_hline(int x0, int y0, int w, uint32_t color) {
      draw_line(x0, y0, x0 + w, y0, color);
    }


    // just draw a bitmap to a position (no scaling)
    void blit(const gfx::point &at, gfx::bitmap &bmp, const gfx::rect &src);


    // draw a "theme frame" that fits with the chariot design style
    void draw_frame(const gfx::rect &r, uint32_t bg, uint32_t border = 0x000000);

    // draw a rectangle border
    void draw_rect(const gfx::rect &r, uint32_t color);
    void fill_rect(const gfx::rect &r, uint32_t color);

    void draw_rect_special(const gfx::rect &r, uint32_t top_left, uint32_t bottom_right);

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
