#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "sys/sysbind.h"
#include "ui/event.h"
#include "ui/textalign.h"
#include <ck/timer.h>
#include <ui/frame.h>
#include <ck/pair.h>
#include <ui/label.h>
#include <ck/json.h>
#include <ck/time.h>


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


class ct_window : public ui::window {
 public:
  ct_window(void) : ui::window("current test", 400, 400) {}

  virtual ck::ref<ui::view> build(void) {
    // clang-format off

    ui::style st = {
      .background = 0xE0E0E0,
      .margins = 10,
      .padding = 10,
    };

    return ui::make<ui::vbox>({
      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Top Left", ui::TextAlign::TopLeft),
          ui::make<ui::label>("Top Center", ui::TextAlign::TopCenter),
          ui::make<ui::label>("Top Right", ui::TextAlign::TopRight),
        }),

      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Left", ui::TextAlign::CenterLeft),
          ui::make<ui::label>("Center", ui::TextAlign::Center),
          ui::make<ui::label>("Right", ui::TextAlign::CenterRight),
        }),

      
      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Bottom Left", ui::TextAlign::BottomLeft),
          ui::make<ui::label>("Bottom Center", ui::TextAlign::BottomCenter),
          ui::make<ui::label>("Bottom Right", ui::TextAlign::BottomRight),
        }),
  
    });

    // clang-format on
  }
};



int main(int argc, char** argv) {
  // ck::json::value val = "hello\"";
  // printf("val: %s\n", val.format().get());
  // sysbind_shutdown();
  // return 0;

  // while (1) {
  //   printf("now: %lld\n", ck::time::ms());
  // }

  ui::application app;
  ct_window* win = new ct_window;

  app.start();
  return 0;
}
