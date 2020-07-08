#pragma once

#include <lumen/msg.h>
#include <stdint.h>

namespace ui {


  struct event {
   public:
    enum class type : int {
      mouse,
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



#undef EV
}  // namespace ui
