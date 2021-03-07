#include <ck/timer.h>
#include <gfx/font.h>
#include <ui/application.h>


#define WIDTH 40
#define HEIGHT 40
#define BOARDSIZE (WIDTH * HEIGHT)

#define SCALE 8

#define TICKS 10

class gol : public ui::view {
  bool init[WIDTH][HEIGHT];

  bool current[WIDTH][HEIGHT];
  bool next[WIDTH][HEIGHT];
  int alive = 0;


  ck::ref<ck::timer> tick_timer;

 public:
  gol() {
    set_flex_grow(1);
    tick_timer = ck::timer::make_interval(1000 / TICKS, [this] { this->tick(); });
    tick_timer->stop();

    memset(init, 0, sizeof(init));
  }
  ~gol(void) {
  }

  int get_neighbours(int i, int j, int l_size, int c_size) {
    int n = 0;
    n += current[i][(j + 1) > (c_size - 1) ? 0 : (j + 1)];
    n += current[i][(j - 1) >= 0 ? (j - 1) : (c_size - 1)];
    n += current[(i + 1) > (l_size - 1) ? 0 : (i + 1)][j];
    n += current[(i - 1) >= 0 ? (i - 1) : (l_size - 1)][j];
    n += current[(i + 1) > (l_size - 1) ? 0 : (i + 1)][(j + 1) > (c_size - 1) ? 0 : (j + 1)];
    n += current[(i + 1) > (l_size - 1) ? 0 : (i + 1)][(j - 1) >= 0 ? (j - 1) : (c_size - 1)];
    n += current[(i - 1) >= 0 ? (i - 1) : (l_size - 1)][(j + 1) > (c_size - 1) ? 0 : (j + 1)];
    n += current[(i - 1) >= 0 ? (i - 1) : (l_size - 1)][(j - 1) >= 0 ? (j - 1) : (c_size - 1)];

    return n;
  }

  void tick(void) {
    memset(next, 0, sizeof(next));
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        int n = get_neighbours(x, y, WIDTH, HEIGHT);
        if (current[x][y]) {  // alive
          next[x][y] = (n == 3 || n == 2);
        } else {  // dead cell
          next[x][y] = (n == 3);
        }
      }
    }
    memcpy(current, next, sizeof(current));
    alive = 0;
    // for (int i = 0; i < WIDTH * HEIGHT; i++) alive += ((bool*)current)[i];
    repaint();
  }

  virtual void paint_event(void) override {
    auto scribe = get_scribe();
    bool is_running = tick_timer->running();
    // scribe.clear();


    // draw all the dudes
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        // int color = is_running ? 0xFFFFFF : 0x777777;
        int color = 0xFFFFFF;
        if ((is_running ? current : init)[x][y]) {
          color = 0x000000;
        }


        for (int ox = 0; ox < SCALE; ox++) {
          for (int oy = 0; oy < SCALE; oy++) {
            int d = color;
            if (!is_running && (ox == 0 || oy == 0)) d = 0;
            int color = (x ^ y) | ((x ^ y) << 8) | ((x ^ y) << 16);
            scribe.draw_pixel(x * SCALE + ox, y * SCALE + oy, d);
          }
        }
      }
    }
  }


  int set_to = 1;
  virtual void mouse_event(ui::mouse_event& ev) override {
    auto p = ev.pos();
    if (within_self(p)) {
      if (ev.pressed & LUMEN_MOUSE_LEFT_CLICK) {
        set_to = !init[ev.x / SCALE][ev.y / SCALE];
      }

      if (ev.left) {
        init[p.x() / SCALE][p.y() / SCALE] = set_to;
        repaint();
      }
    }
  }


  virtual void on_keydown(ui::keydown_event& ev) override {
    if (ev.code == key_space) {
      if (tick_timer->running()) {
        tick_timer->stop();
      } else {
        memcpy(current, init, sizeof(current));
        tick_timer->start(1000 / TICKS, true);
      }
      repaint();
    }
    // reset lol
    if (ev.code == key_r) {
      memset(init, 0, sizeof(init));
      repaint();
    }
  }
};




int main() {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("Game of Life", WIDTH * SCALE, HEIGHT * SCALE);

  auto& game = win->set_view<gol>();
  (void)game;

  // start the application!
  app.start();

  return 0;
}
