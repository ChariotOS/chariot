#include <ck/eventloop.h>
#include <ck/option.h>
#include <ck/timer.h>
#include <gfx/bitmap.h>
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

int main(int argc, char **argv) {
  // connect to the window server
  gui::application app;

  char buf[50];
  sprintf(buf, "window on pid %d", getpid());


  ck::vec<ck::ref<gui::window>> windows;
  // auto window = app.new_window(buf, 300, 200);

  auto win = app.new_window(buf, 400, 400);


  int c = 0;
  auto t = ck::timer::make_interval(30, [&] {
    gfx::scribe s(win->bmp());

		// efficient clear(?)
		s.clear(c);
    win->flush();
		c += 8;
  });


  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();

  return 0;
}
