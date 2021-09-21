#include <lumen-server/WindowManager.h>
#include <gfx/scribe.h>
#include "gfx/image.h"
#include "sys/sysbind.h"

namespace server {

  WindowManager::WindowManager() : m_display(*this) {}


  WindowManager::~WindowManager() {}




  void WindowManager::on_connection(ck::ref<ApplicationConnection> c) {
    // notify the application of this window manager
    c->set_window_manager(this);
  }

  void WindowManager::on_disconnection(ck::ref<ApplicationConnection> c) {}


  void WindowManager::feed_mouse_packet(mouse_packet &pkt) {
    auto old_pos = m_mouse_pos;
    //   printf("mouse packet %d,%d!\n", p.x, p.y);
    if (pkt.is_relative) {
      m_mouse_pos.set_x(m_mouse_pos.x() + pkt.x);
      m_mouse_pos.set_y(m_mouse_pos.y() + pkt.y);
    } else {
      int pos_x, pos_y;
      pos_x = (pkt.x / (float)0xFFFF) * m_display.width();
      pos_y = (pkt.y / (float)0xFFFF) * m_display.height();

      // printf("%d,%d: x:%d, y:%d\n", pkt.x, pkt.y, pos_x, pos_y);
      m_mouse_pos.set_x(pos_x);
      m_mouse_pos.set_y(pos_y);
    }


    static auto cursor = gfx::load_png("/usr/res/icons/pointer.png");
    gfx::bitmap bmp(m_display.width(), m_display.height(), m_display.get_front_buffer_unsafe());
    gfx::scribe s(bmp);
    s.clear(0xFFFFFFFF);

    // s.draw_quadratic_bezier(gfx::point(0, 0), old_pos, m_mouse_pos, 0xFFFFFF);
    s.draw_line_antialias(old_pos, m_mouse_pos, 0);
    int mouse_x = m_mouse_pos.x() - (cursor->width() / 2);
    int mouse_y = m_mouse_pos.y() - (cursor->height() / 2);


    s.blit_alpha({mouse_x, mouse_y}, *cursor, cursor->rect());
    // s.fill_rect(gfx::rect(m_mouse_pos.x(), m_mouse_pos.y(), 10, 10), rand());

    // m_display.flush_fb(FlushWithMemcpy::Yes);

    // mouse_moved = true;
  }


  void WindowManager::feed_keyboard_packet(keyboard_packet_t &p) {
    if (p.ctrl() && p.shift() && p.key == key_f4) sysbind_shutdown();
    printf("keyboard packet %d\n", p.key);
  }

}  // namespace server