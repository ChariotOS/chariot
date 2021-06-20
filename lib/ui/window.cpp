#include <gfx/scribe.h>
#include <sys/sysbind.h>
#include <ui/application.h>
#include <ui/event.h>
#include <ui/icon.h>
#include <ui/window.h>
#include <gfx/disjoint_rects.h>

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
  // set_flex_padding(ui::edges(TITLE_HEIGHT, PADDING, PADDING, PADDING));

  m_frame_font = gfx::font::get("OpenSans SemiBold");
  // m_icon_font = gfx::font::get("feather");

  set_foreground(0x4a4848);
  set_background(0xEFEFEF);
}


ui::windowframe::~windowframe(void) {}


void ui::windowframe::custom_layout(void) {
  if (m_children.size() == 0) return;
  assert(m_children.size() == 1);

  ck::unique_ptr<ui::view> &child = m_children[0];
  auto area = this->rect();
  area.take_from_top(TITLE_HEIGHT);
  // printf("windowframe custom layout: %d %d %d %d\n", area.x, area.y, area.w, area.h);
  child->set_relative_rect(area);
}

void ui::windowframe::set_theme(uint32_t bg, uint32_t fg, uint32_t border) {
  set_background(bg);
  set_foreground(fg);
  // set_bordercolor(border);
  update();
}


void ui::windowframe::mouse_event(ui::mouse_event &ev) {
  if (ev.left) {
    struct lumen::move_request movreq {
      .id = ((ui::window *)surface())->id(), .dx = ev.dx, .dy = ev.dy,
    };
    ui::application::get().send_msg(LUMEN_MSG_MOVEREQ, movreq);
  }
}

void ui::windowframe::paint_event(gfx::scribe &s) {
  /* Draw the title within the titlebar */
  gfx::rect r = {0, 0, width(), TITLE_HEIGHT};
  // r.w = width();

  s.fill_rect(r, 0xEEEEEE);

  m_frame_font->with_line_height(12, [&]() {
    s.draw_text(*m_frame_font, r, ((ui::window *)surface())->m_name, ui::TextAlign::Center,
                get_foreground(), true);
  });

  for (auto &child : m_children) {
    s.draw_rect(child->rect(), child->get_background());
  }
}




ui::window::window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap> bmp) {
  m_id = id;
  m_name = name;
  m_rect = r;
  m_bitmap = bmp;

  m_frame = new ui::windowframe();
  m_frame->m_surface = this;
  m_frame->m_parent = NULL;

  schedule_reflow();
}


ui::window::~window(void) {}



void ui::window::actually_do_invalidations(void) {
  if (m_pending_invalidations.size() == 0) return;
  // printf("actually do %d invalidation(s)\n", m_pending_invalidations.size());

  auto &app = ui::application::get();
  struct lumen::invalidated_msg response = {0};
  struct lumen::invalidate_msg iv;
  iv.id = m_id;

  auto *rv = root_view();
  if (rv != NULL) {
    for (auto &rect : m_pending_invalidations.rects()) {
      gfx::rect rect_copy = rect;
      rv->repaint(rect_copy);
    }
  }


  int n = 0;
  for (auto &rect : m_pending_invalidations.rects()) {
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
}


void ui::window::invalidate(const gfx::rect &r) {
  m_pending_invalidations.add(r);
  ck::eventloop::defer_unique("ui::window::invalidate",
                              [this](void) { actually_do_invalidations(); });
}


ui::view *ui::window::root_view(void) { return m_frame.get(); }

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

    ev.ds = 0;
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



void ui::window::did_reflow(void) { this->m_pending_reflow = false; }


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

  // tell the main view to reflow
  schedule_reflow();
  return ck::tuple(width(), height());
}
