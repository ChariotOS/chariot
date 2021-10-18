#include <lumen-server/WindowManager.h>
#include <gfx/scribe.h>
#include "gfx/image.h"
#include "sys/sysbind.h"
#include <ck/time.h>
#include <gfx/font.h>
#include <math.h>



ck::ref<gfx::bitmap> build_shadow_bitmap(int width, int height, uint32_t shadow_color, int radius, float shadow_intensity = 1.0f) {
  ck::time::logger l("shadow build");
  gfx::rect object = gfx::rect(0, 0, width, height);
  auto r = object;
  r.grow(radius);
  ck::ref<gfx::bitmap> shadow_bitmap = new gfx::bitmap(r.w, r.h);

  auto& b = *shadow_bitmap;
  b.clear(0xFF'FFFFFF);

  if (shadow_intensity > 1) shadow_intensity = 1.0;
  if (shadow_intensity < 0) shadow_intensity = 0.0;


  shadow_color &= 0xFFFFFF;

  gfx::scribe s(b);
  auto shadow_bound = object.shifted(radius, radius);
  s.fill_rect(shadow_bound, 0xFF'000000);

  if (radius > 0) {
    s.stackblur(radius, b.rect());
  }

  auto p = b.pixels();
  for (int i = 0; i < b.width() * b.height(); i++) {
    auto c = p[i];
    uint32_t r = (255 - (c & 0xFF)) * shadow_intensity;
    p[i] = (r << 24) | shadow_color;
  }



  return shadow_bitmap;
}

namespace server {

  WindowManager::WindowManager() : m_display(*this) {
    m_compositor_timer = ck::timer::make_interval(1000 / 60, [this] {
      this->compose();
    });
  }


  WindowManager::~WindowManager() {}




  void WindowManager::on_connection(ck::ref<ApplicationConnection> c) {
    // notify the application of this window manager
    c->set_window_manager(this);
  }

  void WindowManager::on_disconnection(ck::ref<ApplicationConnection> c) {}


  void WindowManager::feedMousePacket(mouse_packet& pkt) {
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




    // ck::time::logger l("mxouse blit");
    static auto cursor = gfx::load_png("/usr/res/icons/pointer.png");
    int mouse_x = m_mouse_pos.x() - (cursor->width() / 2);
    int mouse_y = m_mouse_pos.y() - (cursor->height() / 2);

    gfx::bitmap bmp(m_display.width(), m_display.height(), m_display.get_front_buffer_unsafe());
    gfx::scribe s(bmp);
    s.clear(0xEEEEEE);

    s.blit_alpha(gfx::point(mouse_x, mouse_y), *cursor, cursor->rect());



    // {
    //   auto r = gfx::rect(mouse_x, mouse_y, 256, 256);
    //   int dist = m_display.height() - mouse_y;
    //   int radius = MIN((m_display.height() - mouse_y) / 10, 150);
    //   int ox = 0;
    //   int oy = 64;
    //   auto shadow = build_shadow_bitmap(r.w, r.h, 0x005fb8, radius, 0.15);

    //   auto shadow_r = r;
    //   shadow_r.grow(radius);

    //   s.blit_alpha(gfx::point(shadow_r.x + ox, shadow_r.y + oy), *shadow, shadow->rect());
    //   s.fill_rect(r, 0xFFFFFF);
    // }
  }


  void WindowManager::feedKeyboadPacket(keyboard_packet_t& p) {
    if (p.ctrl() && p.shift() && p.key == key_f4) sysbind_shutdown();
    printf("keyboard packet %d\n", p.key);
  }



  void WindowManager::show(ck::ref<Window> w) {}

  void WindowManager::hide(ck::ref<Window> w) {}


  void WindowManager::compose(void) {
    // printf("compose!\n");
  }

}  // namespace server
