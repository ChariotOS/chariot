#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "ui/event.h"


class blinky : public ui::view {
 public:
  blinky(uint32_t color) : main_color(color) { set_background(color); }
  virtual ~blinky() = default;


  virtual void mouse_event(ui::mouse_event& ev) {
    if (ev.left && !clicked) {
      set_background(0xFF0000);
      clicked = true;
      update();
    } else if (!ev.left && clicked) {
      clicked = false;
      set_background(main_color);
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

int main(int argc, char** argv) {
  ui::application app;
  ui::window* win = app.new_window("Test", 400, 400);
  // win->defer_invalidation(false);

  auto& root = win->set_view<ui::view>();
  root.set_layout<ui::hboxlayout>();
  root.set_padding(5);
  root.layout()->set_spacing(5);


  {
    auto& c = root.add(new blinky(0x0000FF));
    c.set_layout<ui::vboxlayout>();
    // c.layout()->set_spacing(5);

    c.add(create_box(0x29339B, 25));
    c.add(create_box(0x74A4BC));
    {
      // auto* v = create_box(0xB6D6CC, 100);
      auto* v = create_box(0x00FF00, 100);
      ui::edges ed;
      ed.right = 20;
      ed.left = 5;
      ed.top = 10;
      ed.bottom = 4;
      v->set_margins(ed);
      c.add(v);
    }

    c.add(create_box(0xF1FEC6, 25));
    c.set_padding(10);
    // c.set_fixed_width(100);

    c.update();
  }

  {
    auto& c = root.add(new blinky(0x0000FF));
    c.set_layout<ui::vboxlayout>();
    // c.layout()->set_spacing(5);

    c.add(create_box(0x29339B, 25));
    c.add(create_box(0x74A4BC));
    {
      // auto* v = create_box(0xB6D6CC, 100);
      auto* v = create_box(0x00FF00, 100);
      // v->set_margins(5);
      c.add(v);
    }

    c.add(create_box(0xF1FEC6, 25));
    c.set_padding(10);
    // c.set_fixed_width(100);

    c.update();
  }



  // win->schedule_reflow();


  app.start();
  return 0;
}
