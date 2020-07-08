#include <gui/application.h>
#include <gui/window.h>


gui::window::window(int id, ck::string name, gfx::rect r,
                    ck::ref<gfx::shared_bitmap> bmp) {
  m_id = id;
  m_name = name;
  m_rect = r;
  m_bitmap = bmp;
}


gui::window::~window(void) {}

void gui::window::flush(void) {
  auto &app = gui::application::get();
  struct lumen::invalidate_msg iv;
  iv.id = m_id;
  iv.x = 0;
  iv.y = 0;
  iv.w = m_rect.w;
  iv.h = m_rect.h;
  app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
}

void gui::window::handle_input(struct lumen::input_msg &msg) {
  if (msg.type == LUMEN_INPUT_KEYBOARD) {
  } else if (msg.type == LUMEN_INPUT_MOUSE) {
		m_bitmap->set_pixel(msg.mouse.xpos, msg.mouse.ypos, 0xFF00FF);
  }
}
