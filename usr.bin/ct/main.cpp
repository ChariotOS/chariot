#include <chariot/fs/magicfd.h>
#include <ck/rand.h>
#include <ck/tuple.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ui/application.h>
#include <ui/label.h>



int main(int argc, char** argv) {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("My Window", 250, 180);

  auto &root = win->set_view<ui::stackview>(ui::direction::horizontal);
	for (int i = 0; i < 3; i++) {
		char c[32];
		snprintf(c, 32, "Label %d", i);
		auto &lbl = root.spawn<ui::label>(c, ui::TextAlign::Center);
		lbl.set_background(rand());
	}

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();


  return 0;
}
