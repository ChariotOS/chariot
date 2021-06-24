#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "sys/sysbind.h"
#include "ui/event.h"
#include <ck/timer.h>
#include <ui/frame.h>
#include <ck/pair.h>

class blinky : public ui::view {
 public:
  blinky(uint32_t color) : main_color(color) {
    set_background(color);
    // set_margins(5);
  }
  virtual ~blinky() = default;


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
  ui::view* v = new blinky(c);
  // v->set_border(0x000000, 2);
  return v;
}


static void reset_window(ui::window* win) {
  // printf("\n\n\n\nresetting!\n");
  auto& root = win->set_view<ui::view>();

  auto& l = root.set_layout<ui::hboxlayout>();
  l.set_spacing(0);

  int count = 5;
  for (int i = 0; i < count; i++) {
    auto& c = root.add(new ui::frame());
    c.set_layout<ui::vboxlayout>();

    for (int j = 0; j < count; j++) {
      auto& d = c.add(create_box(0xFFFFFF));
      // d.set_padding(20);
    }
  }
}


ck::pair<int, int> floor_ceil(float x) { return ck::pair(floor(x), ceil(x)); }

int main(int argc, char** argv) {
  // {
  //   auto p = floor_ceil(3.14);
  //   auto [f, c] = p;
  //   printf("f: %d, c: %d\n", f, c);
  // }

  // return 0;

  ui::application app;
  ui::window* win = app.new_window("Test", 400, 400);

  win->set_double_buffer(true);

  // win->compositor_sync(true);
  reset_window(win);

  // float v = 0.0;
  // auto t = ck::timer::make_interval(25, [win, &v] {
  //   v += 0.04;
  //   win->resize(400 + 200 * sin(v), 400 + 200 * cos(v));
  //   // reset_window(win);
  // });

  app.start();
  return 0;
}
