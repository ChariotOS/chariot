#include <chariot/fs/magicfd.h>
#include <ck/rand.h>
#include <ck/tuple.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ui/application.h>
#include <ui/color.h>
#include <ui/label.h>



static auto make_label(const char* s, unsigned int fg, unsigned int bg) {
  auto lbl = new ui::label(s, ui::TextAlign::Center);
  lbl->set_foreground(fg);
  lbl->set_background(bg);
	lbl->set_border(0xFF00FF, 1);
  return lbl;

};




int main(int argc, char** argv) {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("My Window", 250, 250);

  auto& root = win->set_view<ui::stackview>(ui::Direction::Vertical);

  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);
  root << make_label("Label", 0, ui::Color::White);


  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();


  return 0;
}
