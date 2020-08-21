#include <gfx/font.h>
#include <ui/application.h>
#include <chariot/fs/magicfd.h>
#include <sys/sysbind.h>
#include <fcntl.h>

#include <pthread.h>

struct line {
  gfx::point start;
  gfx::point end;
};

class painter : public ui::view {
  int x, y;

 public:
  painter(void) {}

  ~painter(void) {}


  virtual void paint_event(void) override {
    auto s = get_scribe();

    s.clear(0xFF'FF'FF);

    auto pr = gfx::printer(s, *gfx::font::get_default(), 0, 0, width());
    pr.set_color(0xFF0000);
    pr.printf("x: %d\n", x);
    pr.printf("y: %d\n", y);

    s.draw_line(x, 0, x, height(), 0xFF0000);
    s.draw_line(0, y, x + width(), y, 0xFF0000);

    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override {
    x = ev.x;
    y = ev.y;
    repaint();
  }
};


int main(int argc, char** argv) {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("My Window", 300, 300);
  // win->set_view<ui::stackview>(ui::direction::horizontal);
  win->set_view<painter>();

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();


  return 0;
}
