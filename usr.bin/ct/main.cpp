#include <ck/eventloop.h>
#include <ck/option.h>
#include <ck/timer.h>
#include <gfx/bitmap.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <gui/application.h>
#include <gui/window.h>
#include <lumen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>



void test(gfx::scribe &s, struct gfx::scribe::text_thunk &t, const char *font,
          int lh) {
  auto fnt = gfx::font::open(font, lh);
  s.draw_text(t, *fnt, "The quick brown fox jumps over the lazy brown dog\n",
              0);
}


int main(int argc, char **argv) {
  // auto fnt = gfx::font::get_default();
	auto fnt = gfx::font::open("chicago-normal", 12);

  while (1) {
    // connect to the window server
    gui::application app;

    ck::vec<gui::window *> windows;

		int width = 350;
    int height = 280;


    char buf[50];
    for (int i = 0; i < 1; i++) {
      sprintf(buf, "This window says, 'Hello, world!'", i);
      auto win = app.new_window(buf, width, height);
      windows.push(win);

      gfx::scribe s(win->bmp());
      s.clear(0xFFFFFF);

      gfx::point p(0, 0);

      auto st = gfx::scribe::text_thunk(0, 0, width);
      s.printf(st, *fnt, 0x000000, 0, "Hello, world");
      win->flush();
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
