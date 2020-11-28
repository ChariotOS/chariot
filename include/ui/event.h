#pragma once

#include <lumen/msg.h>
#include <stdint.h>
#include <chariot/keycode.h>

namespace ui {


  struct event {
   public:
    enum class type : int {
      mouse, keydown, keyup,
    };


    event(ui::event::type type) : m_type(type) {}

    auto get_type(void) const { return m_type; }

		template<typename T>
			inline auto &as() {
				return *static_cast<T*>(this);
			}

   private:
    ui::event::type m_type;
  };

#define EV(klass, t) klass() : ui::event(t) {}

  struct mouse_event : public event {
    int x, y;
		int dx, dy, ds; // change in x, y, scroll
		bool left, right, middle;

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




#undef EV
}  // namespace ui
