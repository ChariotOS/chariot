#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "sys/sysbind.h"
#include "ui/event.h"
#include <ck/timer.h>
#include <ui/frame.h>

class blinky : public ui::view {
 public:
  blinky(uint32_t color) : main_color(color) {
    set_background(color);
    set_margins(5);
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
  v->set_border(0x000000, 2);
  return v;
}

ui::view* create_box(uint32_t c, uint32_t max_height) {
  ui::view* v = new blinky(c);
  v->set_border(0x000000, 2);
  v->set_max_height(max_height);
  return v;
}


static void reset_window(ui::window* win) {
  auto start = sysbind_gettime_microsecond();
  ck::eventloop::defer([start] {
    auto end = sysbind_gettime_microsecond();
    printf("took %llu us\n", end - start);
  });
  // printf("\n\n\n\nresetting!\n");
  auto& root = win->set_view<ui::view>();

  root.set_layout<ui::hboxlayout>();


  for (int i = 0; i < 6; i++) {
    auto& c = root.add(new ui::frame());
    c.set_layout<ui::vboxlayout>();

    for (int j = 0; j < 6; j++) {
      auto& d = c.add(create_box(rand(), rand() % 400));
      d.set_padding(20);
    }
  }
}




int main(int argc, char** argv) {
  ui::application app;
  ui::window* win = app.new_window("Test", 900, 700);


  win->compositor_sync(true);
  reset_window(win);

  // auto t = ck::timer::make_interval(16, [win] { reset_window(win); });

  app.start();
  return 0;
}
