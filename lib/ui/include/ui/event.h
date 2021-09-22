#pragma once

#include <lumen/msg.h>
#include <stdint.h>
#include <chariot/keycode.h>
#include <gfx/point.h>

namespace ui {


  struct event {
   public:
    enum class type : int {
      mouse,
      keydown,
      keyup,
    };


    event(ui::event::type type) : m_type(type) {}

    auto get_type(void) const { return m_type; }

    template <typename T>
    inline auto &as() {
      return *static_cast<T *>(this);
    }

   private:
    ui::event::type m_type;
  };

#define EV(klass, t) \
  klass() : ui::event(t) {}

  struct mouse_event : public event {
    /* If left or right is pressed, x and y are the location of the press and ox and oy
     * are the current offset from the press.
     *
     * If the buttons are not pressed, x and y are the current cursor position. ox and
     * oy will be zero in this situation
     */
    int x, y;
    int ox, oy;
    int dx, dy, ds;  // change in x, y, scroll
    bool left, right, middle;

    int pressed;  /* Newly pressed buttons */
    int released; /* Newly released buttons */
    /* The absolute position of the mouse event (within the view's rect, obv) */
    inline gfx::point pos(void) { return gfx::point(x + ox, y + oy); }

   public:
    EV(mouse_event, ui::event::type::mouse);
  };


  struct keydown_event : public event {
    char c;
    int flags;
    int code;

   public:
    EV(keydown_event, ui::event::type::keydown);
  };


  struct keyup_event : public event {
    char c;
    int flags;
    int code;

   public:
    EV(keyup_event, ui::event::type::keyup);
  };



  struct animation_event : public event {
   public:
    EV(animation_event, ui::event::type::keyup);
  };


#undef EV
}  // namespace ui
