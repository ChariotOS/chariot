#include <ck/eventloop.h>
#include <ck/option.h>
#include <ck/timer.h>
#include <gfx/bitmap.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <lumen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ui/application.h>
#include <ui/view.h>
#include <ck/unicode.h>
#include <ui/window.h>
#include <unistd.h>

class painter : public ui::view {
  int ox = 0;
  int oy = 0;
  bool active = false;

  int color = 0;

 public:
  painter(void) { color = rand(); }


  virtual void repaint(void) override {
    auto s = get_scribe();


    s.clear(color);

    auto st = gfx::scribe::text_thunk(0, 0, s.width());
  	auto fnt = gfx::font::open("chicago-normal", 12);
		s.printf(st, *fnt, 0x000000, 0, "OS Dev\nCommunity");

    invalidate();
  }

  virtual bool mouse_event(ui::mouse_event &ev) override {
    active = ev.left;

    if (ev.right) {
      auto s = get_scribe();
      s.clear(color);
      invalidate();
    }


    if (ev.left) {
      if (!active) {
        ox = ev.x;
        oy = ev.y;
      }
      auto s = get_scribe();

      s.draw_line_antialias(ox, oy, ev.x, ev.y, 0);
      invalidate();
      active = true;
    } else {
      active = false;
    }

    ox = ev.x;
    oy = ev.y;

    return true;
  }
};



int main(int argc, char **argv) {
  // auto fnt = gfx::font::get_default();
  auto fnt = gfx::font::open("chicago-normal", 12);

  printf("%zu\n", sizeof(painter));

  while (1) {
    // connect to the window server
    ui::application app;

    ck::vec<ui::window *> windows;

    int width = 100;
    int height = 100;


    char buf[50];
    for (int i = 0; i < 5; i++) {
      sprintf(buf, "OSDev", i);
      auto win = app.new_window(buf, width, height);
      windows.push(win);
      win->set_view<painter>();
    }

    auto input = ck::file::unowned(0);
    input->on_read([&] {
      getchar();
      ck::eventloop::exit();
    });

    // auto t = ck::timer::make_timeout(1, [] { ck::eventloop::exit(); });

    // start the application!
    app.start();

    windows.clear();
  }


  return 0;
}
