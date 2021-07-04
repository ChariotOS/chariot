#include <gfx/scribe.h>
#include <sys/sysbind.h>
#include <ui/application.h>
#include <ui/event.h>
#include <ui/icon.h>
#include <ui/window.h>
#include <gfx/disjoint_rects.h>
#include <ck/eventloop.h>

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

  ck::ref<ui::view> &child = m_children[0];
  auto area = this->rect();
  area.take_from_top(TITLE_HEIGHT);
  // printf("windowframe custom layout: %d %d %d %d\n", area.x, area.y, area.w, area.h);
  child->set_relative_rect(area);
}

void ui::windowframe::set_theme(uint32_t bg, uint32_t fg, uint32_t border) {
  set_background(bg);
  set_foreground(fg);
  set_bordercolor(border);
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

  // s.fill_rect(r, 0xEEEEEE);

  m_frame_font->with_line_height(12, [&]() {
    s.draw_text(*m_frame_font, r, ((ui::window *)surface())->m_name, ui::TextAlign::Center,
        get_foreground(), true);
  });

  for (auto &child : m_children) {
    s.draw_rect(child->rect(), child->get_background());
  }
}




ui::window::window(ck::string name, int w, int h, int flags) {
  auto &app = ui::application::get();

  w += ui::windowframe::PADDING * 2;
  h += ui::windowframe::PADDING + ui::windowframe::TITLE_HEIGHT;
  struct lumen::create_window_msg msg;
  msg.width = w;
  msg.height = h;
  strncpy(msg.name, name.get(), LUMEN_NAMESZ - 1);

  // the response message
  struct lumen::window_created_msg res = {0};

  if (!app.send_msg_sync(LUMEN_MSG_CREATE_WINDOW, msg, &res)) {
    panic("failed to create window on server");
  }

  if (res.window_id >= 0) {
    app.add_window(res.window_id, this);
  } else {
    panic("invalid res.window_id");
  }


  m_id = res.window_id;
  m_name = name;
  m_rect = gfx::rect(0, 0, w, h);
  m_shared_bitmap = gfx::shared_bitmap::get(res.bitmap_name, w, h);

  update_private_buffer();

  m_frame = new ui::windowframe();
  m_frame->m_surface = this;
  m_frame->m_parent = NULL;

  // sadly, we have to defer this, as we rely on the subclass's virtual functions, which you cannot
  // do in the parent constructor. So defer for some time in the future. This shouldn't add much
  // runtime at all, but it feels very hacky :^)
  // TODO: add some kind of "ui::window::init" which is called by the application later on
  ck::eventloop::defer([this] { this->rebuild(); });
}


ui::window::~window(void) {
  auto &app = ui::application::get();

  lumen::delete_window_msg msg;
  msg.id = m_id;
  if (!app.send_msg(LUMEN_MSG_DELETE_WINDOW, msg)) {
    panic("failed to delete window");
  }

  app.remove_window(m_id);
}


void ui::window::rebuild(void) {
  set_view(build());
  schedule_reflow();
}

void ui::window::set_double_buffer(bool to) {
  if (m_double_buffer == to) return;

  m_double_buffer = to;
  update_private_buffer();
}

void ui::window::update_private_buffer(void) {
  // if we dont want to use a double buffer, make sure we dont have one
  if (!m_double_buffer) {
    if (m_private_bitmap) m_private_bitmap.clear();
  } else {
    if (!m_private_bitmap || (m_private_bitmap->width() != m_shared_bitmap->width() ||
                                 m_private_bitmap->height() != m_shared_bitmap->height())) {
      m_private_bitmap =
          ck::make_ref<gfx::bitmap>(m_shared_bitmap->width(), m_shared_bitmap->height());
    }
  }
}

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


  // now, copy the changes from the private bitmap to the shared one
  if (double_buffer()) {
    assert(m_private_bitmap);
    gfx::scribe s(*m_shared_bitmap);
    constexpr bool debug = false;

    if constexpr (debug) {
      // if we are in debug draw mode, (debug == true), then we want to draw a green
      // rectangle around all the dirty regions inside the window. But this requires
      // that previous green rectangles are cleared, meaning we need to copy all the
      // window's content over to the shared buffer, invalidating the entire window.
      s.blit(gfx::point(0, 0), *m_private_bitmap, m_private_bitmap->rect());
      for (auto &rect : m_pending_invalidations.rects()) {
        s.draw_rect(rect, 0x00FF00);
      }
      // invalidate the entire window instead
      m_pending_invalidations.clear();
      m_pending_invalidations.add(m_private_bitmap->rect());
    } else {
      for (auto &rect : m_pending_invalidations.rects()) {
        s.blit(gfx::point(rect.x, rect.y), *m_private_bitmap, rect);
      }
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

  // auto start = sysbind_gettime_microsecond();
  ck::eventloop::defer_unique("ui::window::invalidate", [this](void) {
    actually_do_invalidations();

    // auto end = sysbind_gettime_microsecond();
    // printf("ui::window::invalidate took %llu us\n", end - start);
  });
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


ck::pair<int, int> ui::window::resize(int w, int h) {
  w += windowframe::PADDING * 2;
  h += windowframe::TITLE_HEIGHT + windowframe::PADDING;

  // don't change stuff if you dont need to
  if (width() == w && height() == h) {
    return ck::pair(w, h);
  }


  auto &app = ui::application::get();

  // allocate the messages
  struct lumen::resize_msg m = {0};
  struct lumen::resized_msg response = {0};
  m.id = this->m_id;
  m.width = w;
  m.height = h;

  app.send_msg_sync(LUMEN_MSG_RESIZE, m, &response);

  // failure case: nothing changed, so we just return the old size
  if (response.id != m.id) {
    return ck::pair(width(), height());
  }


  // printf("name: %s\n", response.bitmap_name);
  auto new_bitmap = gfx::shared_bitmap::get(response.bitmap_name, response.width, response.height);
  if (!new_bitmap) printf("NOT FOUND!\n");
  // update the bitmap
  this->m_shared_bitmap = new_bitmap;
  m_rect.w = response.width;
  m_rect.h = response.height;

  update_private_buffer();

  // tell the main view to reflow
  schedule_reflow();
  return ck::pair(width(), height());
}
