#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "sys/sysbind.h"
#include "ui/event.h"
#include <ck/timer.h>
#include <ui/frame.h>
#include <ck/pair.h>

class colorview : public ui::view {
 public:
  colorview(uint32_t color) : main_color(color) {
    set_background(color);
    // set_margins(5);
  }
  virtual ~colorview() = default;


  virtual void mouse_event(ui::mouse_event& ev) {
    if (ev.ds != 0) {
      // set_min_height(ev.ds + min_height());
      set_max_height(ev.ds + height());
      update_layout();
    }
    if (ev.left && !clicked) {
      set_background(0xFF0000);
      clicked = true;
      update();
    } else if (!ev.left && clicked) {
      clicked = false;
      clear_background();
      // set_background(main_color);
      update();
    }
  }

 private:
  bool clicked = false;
  uint32_t main_color;
};

ui::view* create_box(uint32_t c) {
  ui::view* v = new colorview(c);
  // v->set_border(0x000000, 2);
  return v;
}



// idea for a ck::future api?
namespace ck {

  enum class FutureState {
    Pending,
    Fulfilled,
    Rejected,
  };

  // templated on the return type of the future upon completion
  template <typename T>
  class future {
   public:
    constexpr ck::FutureState state(void) const { return m_state; }

   private:
    ck::FutureState m_state = ck::FutureState::Pending;
  };
}  // namespace ck



#define BLD(T, style, children, ...) \
  ui::styled(ui::with_children(ui::make<T>(__VA_ARGS__), children), style)


class ct_window : public ui::window {
 public:
  ct_window(void) : ui::window("current test", 400, 400) {}

  virtual ck::ref<ui::view> build(void) {
    // clang-format off

    return ui::make<ui::vbox>({
      ui::make<colorview>({
          .layout = new ui::hboxlayout(),
          .margins = 30,
          .padding = 10,
        },
        {
          ui::make<colorview>(rand()),
          ui::make<colorview>(rand()),
          ui::make<colorview>(rand()),
        },
        0xFF0000),
  
        ui::make<colorview>({.margins = 5}, rand()),
        ui::make<colorview>(rand()),
    });

    // clang-format on
  }
};


int main(int argc, char** argv) {
  ui::application app;

  // create the window
  ct_window* win = new ct_window;

  auto t = ck::timer::make_interval(30, [&win] {
    if (win != NULL) {
      delete win;
      win = NULL;
    }

    win = new ct_window;
  });

  app.start();
  return 0;
}
