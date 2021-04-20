#include <ck/rand.h>
#include <gfx/font.h>
#include <gfx/image.h>
#include <math.h>
#include <sys/sysbind.h>
#include "internal.h"

#define TITLE_HEIGHT 29
#define PADDING 0


#define BGCOLOR 0xF6F6F6
#define FGCOLOR 0xF0F0F0
#define BORDERCOLOR 0x000000

// #define TITLECOLOR 0x6261A1
#define TITLECOLOR 0xFFFFFF


#define min(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })

#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })



static gfx::rect close_button() {
  return gfx::rect(4, 4, 9, 9);
}


lumen::window::window(int id, lumen::guest &g, int w, int h) : id(id), guest(g) {
  bitmap = ck::make_ref<gfx::shared_bitmap>(w, h);


  set_mode(window_mode::normal);
}

lumen::window::~window(void) {
}

void lumen::window::set_mode(window_mode mode) {
  this->mode = mode;

  this->rect.w = bitmap->width();
  this->rect.h = bitmap->height();
  return;

  switch (mode) {
    case window_mode::normal:
      // no side borders
      this->rect.w = bitmap->width();   // + (PADDING * 2);
      this->rect.h = bitmap->height();  //  + TITLE_HEIGHT + (PADDING * 2);
      break;
  }
}

void lumen::window::translate_invalidation(gfx::rect &r) {
  if (mode == window_mode::normal) {
    return;
  }
}




int lumen::window::handle_mouse_input(gfx::point &r, struct mouse_packet &p) {
  int x = 0;
  int y = 0;
  if (mode == window_mode::normal) {
    x = r.x();
    y = r.y();
  }


  if (x >= 0 && x < (int)bitmap->width() && y >= 0 && y < (int)bitmap->height()) {
    /*
struct lumen::input_msg m;

m.window_id = this->id;
m.type = LUMEN_INPUT_MOUSE;
m.mouse.xpos = x;
m.mouse.ypos = y;
m.mouse.dx = p.dx;
m.mouse.dy = p.dy;
m.mouse.buttons = p.buttons;

guest.send_msg(LUMEN_MSG_INPUT, m);
    */
  }

  if (y < TITLE_HEIGHT) {
    // this region is draggable
    return WINDOW_REGION_DRAG;
  }


  return WINDOW_REGION_NORM;
}




int lumen::window::handle_keyboard_input(keyboard_packet_t &p) {
  struct lumen::input_msg m;
  m.window_id = this->id;
  m.type = LUMEN_INPUT_KEYBOARD;
  m.keyboard.c = p.character;
  m.keyboard.flags = p.flags;
  m.keyboard.keycode = p.key;

  guest.send_msg(LUMEN_MSG_INPUT, m);
  return 0;
}



gfx::rect lumen::window::bounds() {
  return rect;
}


constexpr int clamp(int val, int max, int min) {
  if (val > max) return max;
  if (val < min) return min;
  return val;
}

static constexpr uint32_t brighten(uint32_t color, float amt) {
  uint32_t fin = 0;
  for (int i = 0; i < 3; i++) {
    int off = i * 8;
    int c = (color >> off) & 0xFF;

    c = clamp(c * amt, 255, 0);
    fin |= c << off;
  }

  return fin;
}


inline int isqrt(int n) {
  int g = 0x8000;
  int c = 0x8000;
  for (;;) {
    if (g * g > n) {
      g ^= c;
    }
    c >>= 1;
    if (c == 0) {
      return g;
    }
    g |= c;
  }
}


void lumen::window::draw(gfx::scribe &s) {
  // draw normal window mode.
  if (mode == window_mode::normal) {
    // auto start = sysbind_gettime_microsecond();

    // draw the window bitmap, offset by the title bar
    auto bmp_rect = bitmap->rect();


    int ox = s.translation().x();
    int oy = s.translation().y();

    int width = bitmap->width();
    int height = bitmap->height();

    gfx::rect dst_rect;
    dst_rect.x = s.translation().x();
    dst_rect.y = s.translation().y();
    dst_rect.w = width;
    dst_rect.h = height;

    auto clipped_rect = dst_rect.intersect(s.state().clip);
    if (clipped_rect.is_empty()) return;

    const int first_row = clipped_rect.top() - dst_rect.top();
    const int last_row = clipped_rect.bottom() - dst_rect.top();
    const int first_column = clipped_rect.left() - dst_rect.left();
    const int last_column = first_column + clipped_rect.w;


#ifdef CONFIG_FANCY_WINDOWS
    constexpr int border_radius = 3;
    if constexpr (border_radius > 0) {
      struct corner {
        bool enabled;
        int cx, cy;
        gfx::rect rect;
      };

      struct corner corners[4];

      /* top left */
      corners[0] = {.enabled = false,
                    .cx = border_radius,
                    .cy = border_radius,
                    .rect = gfx::rect(0, 0, border_radius, border_radius)};


      /* top right */
      corners[1] = {.enabled = false,
                    .cx = width - border_radius - 1,
                    .cy = border_radius,
                    .rect = gfx::rect(width - border_radius, 0, border_radius, border_radius)};

      /* bottom left */
      corners[2] = {.enabled = false,
                    .cx = border_radius,
                    .cy = height - border_radius - 1,
                    .rect = gfx::rect(0, height - border_radius, border_radius, border_radius)};


      /* bottom right */
      corners[3] = {.enabled = false,
                    .cx = width - border_radius - 1,
                    .cy = height - border_radius - 1,
                    .rect = gfx::rect(width - border_radius, height - border_radius, border_radius,
                                      border_radius)};

      auto &bmp = *bitmap;
      for (int i = 0; i < 4; i++) {
        auto &corner = corners[i];

        int cx = corner.cx;
        int cy = corner.cy;

        for (int y = corner.rect.top(); y <= corner.rect.bottom(); y++) {
          for (int x = corner.rect.left(); x <= corner.rect.right(); x++) {
            uint32_t color = bmp.get_pixel(x, y);

            int dx = x - cx;
            int dy = y - cy;
            int idist = isqrt(dx * dx + dy * dy);

            if (abs(idist - border_radius) <= 4) {
              // between 0 and 4. How many points in the MSAA check were valid?
              int hits = 0;
              int total = 0;

              auto check = [&](float mx, float my) {
                total++;
                float adx = (x + mx) - (cx + 0.5);
                float ady = (y + my) - (cy + 0.5);
                if (sqrt(adx * adx + ady * ady) < border_radius) hits++;
              };

              constexpr float c = 0.33;
              check(c, c);
              check(c, 1 - c);
              check(1 - c, c);
              check(1 - c, 1 - c);

              int alpha = (hits * 255) / total;

              if (hits != 0) {
                s.blend_pixel(x, y, color, hits / (float)total);
              }
              continue;
            }
            if (idist > border_radius) continue;

            s.draw_pixel(x, y, color);
          }
        }
      }
    }
#endif


    for (int y = first_row; y <= last_row; ++y) {
      const uint32_t *src = bitmap->scanline(y);
      uint32_t *dst = s.bmp.scanline(y + oy) + ox;
#ifdef CONFIG_FANCY_WINDOWS
      if constexpr (border_radius > 0) {
        if (unlikely(y < border_radius || y >= height - border_radius)) {
          for (int x = max(first_column, border_radius);
               x < min(last_column, width - border_radius); x++) {
            dst[x] = src[x];
          }
          continue;
        }
      }
#endif

      memcpy(dst + first_column, src + first_column, clipped_rect.w * sizeof(uint32_t));
    }
  }
}
