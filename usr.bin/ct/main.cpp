#include <gfx/font.h>
#include <ui/application.h>
#include <chariot/fs/magicfd.h>

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


pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;


// A normal C function that is executed as a thread
// when its name is specified in pthread_create()
void* func(void* vargp) {
  pthread_mutex_lock(&m);
  printf("B!\n");
  pthread_mutex_unlock(&m);
  return NULL;
}





__thread int val;


int main(int argc, char** argv) {


	val = 30;
	printf("%p\n", &val);

  return 0;


  pthread_t thread_id;
  printf("Before Thread\n");

  pthread_mutex_lock(&m);
  pthread_create(&thread_id, NULL, func, NULL);
  printf("A\n");
  pthread_mutex_unlock(&m);

  while (1) {
  }

  // pthread_join(thread_id, NULL);
  exit(0);



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
