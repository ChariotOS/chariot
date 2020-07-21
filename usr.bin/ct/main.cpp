#include <gfx/bitmap.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <lumen.h>
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
#include <ck/option.h>
#include <ck/rand.h>
#include <ck/strstream.h>
#include <ck/timer.h>
#include <ck/tuple.h>
#include <ck/unicode.h>


class painter : public ui::view {
  int color = 0;

 public:
  painter(int width = -1) {
    if (width != -1) {
      // set the width
      set_width_policy(ui::size_policy::fixed);
      set_size(ui::direction::horizontal, width);
    }
    color = rand();
  }


  virtual void paint_event(void) override {
    auto s = get_scribe();
    s.clear(color);
    invalidate();
  }

	virtual void on_mouse_move(ui::mouse_event &ev) override {
      auto s = get_scribe();

			int ox = ev.x - ev.dx;
			int oy = ev.y - ev.dy;
      s.draw_line_antialias(ox, oy, ev.x, ev.y, 0);
			invalidate();
	}

	/*
  virtual bool mouse_event(ui::mouse_event& ev) override {
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
	*/
};




struct Thingy {
  int foo(void) { return 42; }
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

    int width = 400;
    int height = 300;


    char buf[50];
    for (int i = 0; i < 1; i++) {
      sprintf(buf, "[Window %d]", i);
      auto win = app.new_window(buf, width, height);
      windows.push(win);
      auto& stk = win->set_view<ui::stackview>(ui::direction::horizontal);


			srand(0);
      stk.spawn<painter>(20);
      stk.spawn<painter>(50);
      stk.spawn<painter>(10);
      stk.spawn<painter>();
      stk.spawn<painter>(50);
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
