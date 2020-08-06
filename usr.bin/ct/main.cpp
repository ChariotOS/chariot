#include <gfx/bitmap.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <lumen.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/emx.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ui/application.h>
#include <ui/view.h>
#include <ui/window.h>
#include <unistd.h>

#include <ck/array.h>
#include <ck/eventloop.h>
#include <ck/mem.h>
#include <ck/option.h>
#include <ck/rand.h>
#include <ck/strstream.h>
#include <ck/timer.h>
#include <ck/tuple.h>
#include <ck/unicode.h>


struct line {
  gfx::point start;
  gfx::point end;
};

class painter : public ui::view {
  int color = 0;

  int x, y;

 public:
  painter(void) {
    //
    color = rand();
  }


  virtual void paint_event(void) override {
    auto s = get_scribe();

    s.clear(0xFFFFFF);

    auto pr = gfx::printer(s, *gfx::font::get_default(), 0, 0, width());
		pr.set_color(0x000000);
    pr.printf("x: %d\n", x);
    pr.printf("y: %d\n", y);

    s.draw_hline(0, y, width(), 0xFF0000);
    s.draw_vline(x, 0, height(), 0xFF0000);

    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override {
    x = ev.x;
    y = ev.y;
    repaint();
  }
};




int main(int argc, char** argv) {
  /*
  int emx = emx_create();

  int foo = 0;
  emx_set(emx, 0, (void*)&foo, EMX_READ);

  int events = 0;
  void *key = emx_wait(emx, &events);

  printf("key=%p, foo=%p\n", key, &foo);

  close(emx);
  return 0;
  */

  // ck::strstream stream;
  // stream.fmt("hello");

  while (1) {
    // connect to the window server
    ui::application app;
    ck::vec<ui::window*> windows;

    int width = 300;
    int height = 300;


    char buf[50];
    for (int i = 0; i < 5; i++) {
      sprintf(buf, "[Window %d]", i);
      auto win = app.new_window(buf, width, height);
      windows.push(win);
      auto& stk = win->set_view<ui::stackview>(ui::direction::horizontal);

      stk.spawn<painter>();
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
