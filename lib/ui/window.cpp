#include <gfx/scribe.h>
#include <sys/sysbind.h>
#include <ui/application.h>
#include <ui/event.h>
#include <ui/icon.h>
#include <ui/window.h>


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


ui::windowframe::windowframe(void) {
  set_flex_padding(ui::edges(TITLE_HEIGHT, PADDING, PADDING, PADDING));

  m_frame_font = gfx::font::get("OpenSans");
  m_icon_font = gfx::font::get("feather");

  set_foreground(0x4a4848);
  set_background(0xEFEFEF);
}


ui::windowframe::~windowframe(void) {
}


void ui::windowframe::mouse_event(ui::mouse_event &ev) {
  if (ev.left) {
    struct lumen::move_request movreq {
      .id = ((ui::window *)surface())->id(), .dx = ev.dx, .dy = ev.dy,
    };

    ui::application::get().send_msg(LUMEN_MSG_MOVEREQ, movreq);
  }
}

void ui::windowframe::paint_event(void) {
  auto s = get_scribe();
  // outside border
  // s.draw_rect(gfx::rect(width(), height()), get_bordercolor());

  /* Draw the title within the titlebar */
  {
    gfx::rect r;
    r.x = 0;
    r.y = 0;
    r.h = TITLE_HEIGHT;
    r.w = width();

    m_frame_font->with_line_height(12, [&]() {
      s.draw_text(*m_frame_font, r, ((ui::window *)surface())->m_name, ui::TextAlign::Center,
                  get_foreground(), true);
    });



    if (1) {
      int lh = 18;
      m_icon_font->with_line_height(lh, [&]() {
        int x = 5;
        int y = lh + 4;
        m_icon_font->draw(x, y, s, ui::icon::x_square, get_foreground());
      });
    }
  }
  invalidate();
}



ui::window::window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap> bmp) {
  m_id = id;
  m_name = name;
  m_rect = r;
  m_bitmap = bmp;

  m_frame = new ui::windowframe();
  m_frame->m_surface = this;
  m_frame->m_parent = NULL;

  m_frame->set_size(m_rect.w, m_rect.h);
  reflow();
}


ui::window::~window(void) {
}

void ui::windowframe::set_theme(uint32_t bg, uint32_t fg, uint32_t border) {
  set_background(bg);
  set_foreground(fg);
  // set_bordercolor(border);
  repaint();
}


void ui::window::invalidate(const gfx::rect &r) {
  if (!m_defer_invalidation) {
    auto &app = ui::application::get();
    struct lumen::invalidate_msg iv;
    iv.sync = this->m_compositor_sync;
    iv.id = m_id;
    iv.nrects = 1;
    iv.rects[0].x = r.x;
    iv.rects[0].y = r.y;
    iv.rects[0].w = r.w;
    iv.rects[0].h = r.h;
    if (iv.sync) {
      struct lumen::invalidated_msg response = {0};
      app.send_msg_sync(LUMEN_MSG_WINDOW_INVALIDATE, iv, &response);
    } else {
      app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
    }
    return;
  }

  if (m_pending_invalidations.size() == 0) {
    auto start_time = sysbind_gettime_microsecond();
    ck::eventloop::defer([this, start_time](void) {
      auto &app = ui::application::get();
      struct lumen::invalidated_msg response = {0};
      struct lumen::invalidate_msg iv;
      iv.id = m_id;

      int nrects = m_pending_invalidations.size();
      auto *start = m_pending_invalidations.data();

      int n = 0;
      for (auto &rect : m_pending_invalidations) {
        iv.sync = this->m_compositor_sync;
        iv.rects[n].x = rect.x;
        iv.rects[n].y = rect.y;
        iv.rects[n].w = rect.w;
        iv.rects[n].h = rect.h;
        n++;
        if (n == MAX_INVALIDATE) {
          iv.nrects = n;
          if (iv.sync) {
            app.send_msg_sync(LUMEN_MSG_WINDOW_INVALIDATE, iv, &response);
          } else {
            app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
          }
          n = 0;
        }
      }
      if (n != 0) {
        iv.nrects = n;
        if (iv.sync) {
          app.send_msg_sync(LUMEN_MSG_WINDOW_INVALIDATE, iv, &response);
        } else {
          app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
        }
        // app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
      }

      m_pending_invalidations.clear();

      if (0)
        printf("invalidation of %d region(s) took %.2fms\n", m_pending_invalidations.size(),
               (sysbind_gettime_microsecond() - start_time) / 1000.0);
    });
  }
  m_pending_invalidations.push(r);
}


ui::view *ui::window::root_view(void) {
  return m_frame.get();
}

gfx::point last_pos;

void ui::window::handle_input(struct lumen::input_msg &msg) {
  if (msg.type == LUMEN_INPUT_KEYBOARD) {
    auto &k = msg.keyboard;

    if (k.flags & LUMEN_KBD_PRESS) {
      ui::keydown_event ev;
      ev.c = k.c;
      ev.flags = k.flags;
      ev.code = k.keycode;
      if (focused) focused->event(ev);
    } else {
      ui::keyup_event ev;
      ev.c = k.c;
      ev.flags = k.flags;
      ev.code = k.keycode;
      if (focused) focused->event(ev);
    }
  } else if (msg.type == LUMEN_INPUT_MOUSE) {
    int clicked = (msg.mouse.buttons & (LUMEN_MOUSE_LEFT_CLICK | LUMEN_MOUSE_RIGHT_CLICK));
    /* What buttons where pressed */
    int pressed = ~mouse_down & clicked;
    /* And which were released */
    int released = mouse_down & ~clicked;
    /* Update the current click state */
    mouse_down = clicked;

    auto pos = gfx::point(msg.mouse.hx, msg.mouse.hy);


    if (pressed & LUMEN_MOUSE_LEFT_CLICK) last_lclick = pos;
    if (pressed & LUMEN_MOUSE_RIGHT_CLICK) last_rclick = pos;

    if ((clicked | released) & LUMEN_MOUSE_LEFT_CLICK) pos = last_lclick;
    if ((clicked | released) & LUMEN_MOUSE_RIGHT_CLICK) pos = last_rclick;


    ui::mouse_event ev;
    ev.x = pos.x();
    ev.y = pos.y();

    ev.ox = msg.mouse.hx - ev.x;
    ev.oy = msg.mouse.hy - ev.y;
    ev.dx = msg.mouse.dx;
    ev.dy = msg.mouse.dy;
    ev.left = (msg.mouse.buttons & LUMEN_MOUSE_LEFT_CLICK) != 0;
    ev.right = (msg.mouse.buttons & LUMEN_MOUSE_RIGHT_CLICK) != 0;
    ev.pressed = pressed;
    ev.released = pressed;

    /*
if (pressed & LUMEN_MOUSE_LEFT_CLICK) printf("pressed left click\n");
if (pressed & LUMEN_MOUSE_RIGHT_CLICK) printf("pressed right click\n");

if (released & LUMEN_MOUSE_LEFT_CLICK) printf("released left click\n");
if (released & LUMEN_MOUSE_RIGHT_CLICK) printf("released right click\n");
    */

    if (msg.mouse.buttons & LUMEN_MOUSE_SCROLL_UP) {
      ev.ds = 1;
    } else if (msg.mouse.buttons & LUMEN_MOUSE_SCROLL_DOWN) {
      ev.ds = -1;
    }
    /* Calculate who gets the mouse event */
    root_view()->event(ev);
    // TODO: forward to the main widget or something
  }
}



void ui::window::did_reflow(void) {
  this->m_pending_reflow = false;
}

void ui::window::schedule_reflow(void) {
  if (!m_pending_reflow) {
    ck::eventloop::defer([this]() { reflow(); });
  }
}

ck::tuple<int, int> ui::window::resize(int w, int h) {
  w += windowframe::PADDING * 2;
  h += windowframe::TITLE_HEIGHT + windowframe::PADDING;

  // don't change stuff if you dont need to
  if (width() == w && height() == h) {
    return ck::tuple(w, h);
  }




  auto &app = ui::application::get();

  // allocate the messages
  struct lumen::resize_msg m = {0};
  struct lumen::resized_msg response = {0};
  m.id = this->m_id;
  m.width = w;
  m.height = h;

  app.send_msg_sync(LUMEN_MSG_RESIZE, m, &response);

  // failure case
  if (response.id != m.id) {
    // nothing changed, so we just return the old size
    return ck::tuple(width(), height());
  }

  auto new_bitmap = gfx::shared_bitmap::get(response.bitmap_name, response.width, response.height);
  if (!new_bitmap) printf("NOT FOUND!\n");
  // update the bitmap
  this->m_bitmap = new_bitmap;
  m_rect.w = width();
  m_rect.h = height();

  m_frame->set_size(m_rect.w, m_rect.h);




  // m_frame->set_pos(0, 0);  // the main widget exists at the top left

  // tell the main vie to reflow
  reflow();
  return ck::tuple(width(), height());
}
