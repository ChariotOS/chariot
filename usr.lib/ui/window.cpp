#include <gfx/scribe.h>
#include <ui/application.h>
#include <ui/event.h>
#include <ui/window.h>


ui::window::window(int id, ck::string name, gfx::rect r,
                   ck::ref<gfx::shared_bitmap> bmp) {
  m_id = id;
  m_name = name;
  m_rect = r;
  m_bitmap = bmp;
}


ui::window::~window(void) {}


void ui::window::invalidate(const gfx::rect &r) {
  auto &app = ui::application::get();
  struct lumen::invalidate_msg iv;
  iv.id = m_id;
  iv.x = r.x;
  iv.y = r.y;
  iv.w = r.w;
  iv.h = r.h;
  // printf("invalidate x:%3d y:%3d w:%3d h:%3d)\n", r.x, r.y, r.w, r.h);
  app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
}

void ui::window::flush(void) { invalidate(m_rect); }

gfx::point last_pos;

void ui::window::handle_input(struct lumen::input_msg &msg) {
  if (msg.type == LUMEN_INPUT_KEYBOARD) {
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
    if (m_main_view) {
      m_main_view->event(ev);
    }
    // TODO: forward to the main widget or something
  }
}


void ui::window::schedule_reflow(void) {
  if (!m_pending_reflow) {
    ck::eventloop::defer(fn() {
      // tell the main vie to reflow
      m_main_view->do_reflow();
      this->m_pending_reflow = false;
    });
  }
}
