#include <ck/eventloop.h>
#include <ck/option.h>
#include <gfx/rect.h>
#include <gui/application.h>
#include <gui/window.h>
#include <lumen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <gfx/bitmap.h>
#include <unistd.h>


int main(int argc, char **argv) {

  // connect to the window server
  gui::application app;

  char buf[50];
  sprintf(buf, "window on pid %d", getpid());


  ck::vec<ck::ref<gui::window>> windows;
  // auto window = app.new_window(buf, 300, 200);

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    char c = getchar();
    if (c == 'n') {
      char buf[50];
      sprintf(buf, "window %d on pid %d", windows.size(), getpid());
			auto win = app.new_window(buf, 300, 200);
			win->bmp().clear(rand());
			// flush the whole window!
			win->flush();
      windows.push(win);
      return;
    }
    ck::eventloop::exit();
  });

  // start the application!
  app.start();

  return 0;
}
