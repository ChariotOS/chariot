#pragma once

#include <lumen-server/ApplicationConnection.h>
#include <lumen-server/Window.h>
#include <lumen-server/Display.h>
#include <ck/string.h>
#include <gfx/point.h>
#include <chariot/mouse_packet.h>
#include <chariot/keycode.h>
#include <ck/timer.h>

namespace server {

  // The WindowManager is the root of the lumen window server hierarchy.
  // It manages the ipc server, applications, windows, etc...
  class WindowManager : public ck::ipc::server<ApplicationConnection> {
   public:
    WindowManager();
    virtual ~WindowManager();


    // ^ck::ipc::server<ApplicationConnection>
    void on_connection(ck::ref<ApplicationConnection> c) override;
    void on_disconnection(ck::ref<ApplicationConnection> c) override;

    void feedMousePacket(mouse_packet &p);
    void feedKeyboadPacket(keyboard_packet_t &p);



    template <typename Fn>
    void for_each_window_front_to_back(Fn fn) {
      for (auto &w : m_window_stack)
        fn(w);
    }

    template <typename Fn>
    void for_each_window_back_to_front(Fn fn) {
      for (int i = m_window_stack.size() - 1; i >= 0; i--)
        fn(m_window_stack[i]);
    }



    // add and remove windows from the internal window stack
    void show(ck::ref<Window> w);
    void hide(ck::ref<Window> w);


    // run the compositor
    void compose(void);


   protected:
    gfx::point m_mouse_pos;

    ck::timer::ref m_compositor_timer;

    Display m_display;
    ck::vec<ck::ref<Window>> m_window_stack;
  };
}  // namespace server