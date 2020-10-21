#include <gfx/scribe.h>
#include <ui/application.h>
#include <ui/event.h>
#include <ui/window.h>


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


ui::windowframe::windowframe(void) : ui::stackview(ui::Direction::Vertical) {
  padding = ui::edges(PADDING);
  padding.top = TITLE_HEIGHT;
  // bordercolor = ui::Color::Black;
  bordersize = 0;

  background = FRAME_COLOR;
}

ui::windowframe::~windowframe(void) {}

void ui::windowframe::paint_event(void) {
  auto s = get_scribe();
  uint32_t border = 0x222222; // 0x666666;
  float contrast = 0.25;
  uint32_t bright = brighten(FRAME_COLOR, 1.0 + contrast);
  uint32_t dark = brighten(FRAME_COLOR, 1.0 - contrast);

  gfx::rect r = gfx::rect(width(), height());
  // outside border
  s.draw_rect(r, border);


  if constexpr (PADDING > 1) {
    r.shrink(1);
    s.draw_rect_special(r, bright, dark);
  }

  r = padded_area();
  r.grow(1);
  s.draw_rect(r, border);

  if constexpr (PADDING > 2) {
    r.grow(1);
    s.draw_rect_special(r, dark, bright);
  }
}




ui::window::window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap> bmp) {
  m_id = id;
  m_name = name;
  m_rect = r;
  m_bitmap = bmp;

  m_frame = new ui::windowframe();
  m_frame->m_window = this;
  m_frame->m_parent = NULL;

  m_frame->set_size(m_rect.w, m_rect.h);
  m_frame->set_pos(0, 0);  // the main widget exists at the top left
  // tell the main vie to reflow
  m_frame->do_reflow();
}


ui::window::~window(void) {}


void ui::window::invalidate(const gfx::rect &r, bool sync) {
  if (!m_defer_invalidation) {
    auto &app = ui::application::get();
    struct lumen::invalidated_msg response = {0};
    struct lumen::invalidate_msg iv;
    iv.id = m_id;
    iv.nrects = 1;
    iv.rects[0].x = r.x;
    iv.rects[0].y = r.y;
    iv.rects[0].w = r.w;
    iv.rects[0].h = r.h;
    app.send_msg_sync(LUMEN_MSG_WINDOW_INVALIDATE, iv, &response);
    return;
  }

  if (m_pending_invalidations.size() == 0) {
    ck::eventloop::defer([this](void) {
      auto &app = ui::application::get();
      struct lumen::invalidated_msg response = {0};
      struct lumen::invalidate_msg iv;
      iv.id = m_id;

      int nrects = m_pending_invalidations.size();
      auto *start = m_pending_invalidations.data();

      int n = 0;
      for (auto &rect : m_pending_invalidations) {
        iv.rects[n].x = rect.x;
        iv.rects[n].y = rect.y;
        iv.rects[n].w = rect.w;
        iv.rects[n].h = rect.h;
        n++;
        if (n == MAX_INVALIDATE) {
          iv.nrects = n;
          app.send_msg_sync(LUMEN_MSG_WINDOW_INVALIDATE, iv, &response);
          n = 0;
        }
      }
      if (n != 0) {
        iv.nrects = n;
        app.send_msg_sync(LUMEN_MSG_WINDOW_INVALIDATE, iv, &response);
      }

      m_pending_invalidations.clear();
    });
  }
  m_pending_invalidations.push(r);
}

void ui::window::flush(void) { invalidate(m_rect); }

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
    ui::mouse_event ev;
    ev.x = msg.mouse.xpos;
    ev.y = msg.mouse.ypos;
    ev.dx = msg.mouse.dx;
    ev.dy = msg.mouse.dy;
    ev.left = (msg.mouse.buttons & LUMEN_MOUSE_LEFT_CLICK) != 0;
    ev.right = (msg.mouse.buttons & LUMEN_MOUSE_RIGHT_CLICK) != 0;

    if (msg.mouse.buttons & LUMEN_MOUSE_SCROLL_UP) {
      ev.ds = 1;
    } else if (msg.mouse.buttons & LUMEN_MOUSE_SCROLL_DOWN) {
      ev.ds = -1;
    }
    if (focused) {
      focused->event(ev);
    }
    // TODO: forward to the main widget or something
  }
}


void ui::window::schedule_reflow(void) {
  if (!m_pending_reflow) {
    ck::eventloop::defer(fn() {
      m_frame->set_size(m_rect.w, m_rect.h);
      m_frame->set_pos(0, 0);  // the main widget exists at the top left
      // tell the main vie to reflow
      m_frame->do_reflow();
      this->m_pending_reflow = false;
    });
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
  m_frame->set_pos(0, 0);  // the main widget exists at the top left
  // tell the main vie to reflow
  m_frame->do_reflow();
  return ck::tuple(w, h);
}
