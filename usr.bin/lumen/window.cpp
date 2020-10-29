#include <gfx/font.h>
#include <gfx/image.h>
#include "internal.h"

#define TITLE_HEIGHT 20
#define PADDING 0


#define BGCOLOR 0xF6F6F6
#define FGCOLOR 0xF0F0F0
#define BORDERCOLOR 0x000000

// #define TITLECOLOR 0x6261A1
#define TITLECOLOR 0xFFFFFF



static gfx::rect close_button() { return gfx::rect(4, 4, 9, 9); }


lumen::window::window(int id, lumen::guest &g, int w, int h) : id(id), guest(g) {
  bitmap = ck::make_ref<gfx::shared_bitmap>(w, h);


  set_mode(window_mode::normal);
}

lumen::window::~window(void) {}

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

  if (y < TITLE_HEIGHT) {
    // this region is draggable
    return WINDOW_REGION_DRAG;
  }

  if (x >= 0 && x < (int)bitmap->width() && y >= 0 && y < (int)bitmap->height()) {
    struct lumen::input_msg m;

    m.window_id = this->id;
    m.type = LUMEN_INPUT_MOUSE;
    m.mouse.xpos = x;
    m.mouse.ypos = y;
    m.mouse.dx = p.dx;
    m.mouse.dy = p.dy;
    m.mouse.buttons = p.buttons;

    guest.send_msg(LUMEN_MSG_INPUT, m);
    return WINDOW_REGION_NORM;
  }


  return 0;
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



gfx::rect lumen::window::bounds() { return rect; }


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


void lumen::window::draw(gfx::scribe &scribe) {
  // draw normal window mode.
  if (mode == window_mode::normal) {
    // draw the window bitmap, offset by the title bar
    auto bmp_rect = bitmap->rect();

    scribe.blit(gfx::point(0, 0), *bitmap, bmp_rect);


    if constexpr (0) {
      auto scaled = bitmap->scale(bitmap->width() / 4, bitmap->height() / 4, gfx::bitmap::SampleMode::Nearest);
      scribe.blit(gfx::point(50, 50), *scaled, scaled->rect());
    }

#define BUTTON_PADDING 4
    gfx::rect button(0, 0, 21, 21);

    // button.x = rect.w - button.w;

    button.shrink(1);
    button.shrink(BUTTON_PADDING);


    scribe.draw_frame(button, 0xFFFFFF, 0x666666);
    // scribe.fill_rect(button, 0xED6B5F);
    // scribe.draw_rect(button, brighten(0xED6B5F, 0.8));
    return;

    // scribe.draw_line(9, 6, 17, 14, 0);
    // scribe.draw_line(9, 14, 17, 6, 0);


    button.x -= button.w + BUTTON_PADDING;
    scribe.fill_rect(button, 0xF4BF50);
    scribe.draw_rect(button, brighten(0xF4BF50, 0.8));


    button.x -= button.w + BUTTON_PADDING;
    scribe.fill_rect(button, 0x63C756);
    scribe.draw_rect(button, brighten(0x63C756, 0.8));
    return;
  }

  return;
}
