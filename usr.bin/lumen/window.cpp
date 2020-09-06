#include <gfx/font.h>
#include <gfx/image.h>
#include "internal.h"

#define TITLE_HEIGHT 19
#define PADDING 4


#define BGCOLOR 0xF6F6F6
#define FGCOLOR 0xF0F0F0
#define BORDERCOLOR 0x000000

// #define TITLECOLOR 0x6261A1
#define TITLECOLOR 0xFFFFFF


auto title_font(void) {
  static auto fnt = gfx::font::open("chicago-normal", 12);
  return fnt;
}


static gfx::rect close_button() { return gfx::rect(4, 4, 9, 9); }


lumen::window::window(int id, lumen::guest &g, int w, int h) : id(id), guest(g) {
  bitmap = ck::make_ref<gfx::shared_bitmap>(w, h);


  set_mode(window_mode::normal);
}

lumen::window::~window(void) {}

void lumen::window::set_mode(window_mode mode) {
  this->mode = mode;
  switch (mode) {
    case window_mode::normal:
      // no side borders
      this->rect.w = bitmap->width() + (PADDING * 2);
      this->rect.h = bitmap->height() + TITLE_HEIGHT + (PADDING * 2);
      break;
  }
}

void lumen::window::translate_invalidation(gfx::rect &r) {
  if (mode == window_mode::normal) {
    r.x += PADDING;
    r.y += TITLE_HEIGHT + PADDING;
  }
}


int lumen::window::handle_mouse_input(gfx::point &r, struct mouse_packet &p) {
  int x = 0;
  int y = 0;
  if (mode == window_mode::normal) {
    x = r.x() - PADDING;
    y = r.y() - TITLE_HEIGHT - PADDING;
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
  if (y < 0) {
    if (close_button().contains(r.x(), r.y())) {
      // The region describes the close region
      return WINDOW_REGION_CLOSE;
    } else {
      // this region is draggable
      return WINDOW_REGION_DRAG;
    }
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


void lumen::window::draw(gfx::scribe &scribe) {
  // draw normal window mode.
  if (mode == window_mode::normal) {
    // draw the window bitmap, offset by the title bar
    auto bmp_rect = gfx::rect(0, 0, bitmap->width(), bitmap->height());


    {
      auto frame = gfx::rect(0, 0, rect.w - 1, rect.h - 1);

      for (int i = 0; i < PADDING - 1; i++) {
        scribe.draw_rect(frame, i == 0 ? BORDERCOLOR : BGCOLOR);
        frame.x++;
        frame.y++;
        frame.w -= 2;
        frame.h -= 2;
      }
      scribe.draw_rect(frame, BORDERCOLOR);
    }

    // The border rectangles
    int rsiz = PADDING + TITLE_HEIGHT - 1;
    // Top Left
    scribe.draw_rect(gfx::rect(0, 0, rsiz, rsiz), BORDERCOLOR);
    // Top Right
    scribe.draw_rect(gfx::rect(rect.w - rsiz - 1, 0, rsiz, rsiz), BORDERCOLOR);

    // Bottom Left
    scribe.draw_rect(gfx::rect(0, rect.h - rsiz - 1, rsiz, rsiz), BORDERCOLOR);
    // Bottom Right
    scribe.draw_rect(gfx::rect(rect.w - rsiz - 1, rect.h - rsiz - 1, rsiz, rsiz), BORDERCOLOR);


		// TITLE
    scribe.fill_rect(gfx::rect(PADDING, PADDING, rect.w - PADDING * 2, TITLE_HEIGHT), TITLECOLOR);
		scribe.draw_hline(0, PADDING + TITLE_HEIGHT - 1, rect.w - 1, BORDERCOLOR);



    scribe.blit(gfx::point(PADDING, PADDING + TITLE_HEIGHT), *bitmap, bmp_rect);



    // scribe.blit(gfx::point(1, TITLE_HEIGHT), *bitmap, bmp_rect);
    // scribe.draw_frame(gfx::rect(0, 0, rect.w, TITLE_HEIGHT), BGCOLOR, FGCOLOR);
    // scribe.draw_rect(close_button(), FGCOLOR);

#if 1
    auto pr = gfx::printer(scribe, *title_font(), 25, PADDING, rect.w);
    pr.set_color(BORDERCOLOR);
    pr.printf("%s", name.get());
#endif

    // scribe.draw_line(0, TITLE_HEIGHT, 0, rect.h - 2, FGCOLOR);
    // scribe.draw_line(rect.w - 1, TITLE_HEIGHT, rect.w - 1, rect.h - 2, FGCOLOR);
    // scribe.draw_line(0, rect.h - 2, rect.w - 1, rect.h - 2, FGCOLOR);

    return;
  }

  return;
}
