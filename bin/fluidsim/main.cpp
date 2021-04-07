#include <ck/timer.h>
#include <gfx/font.h>
#include <ui/application.h>
#include <sys/sysbind.h>
#include <unistd.h>
#include <sys/wait.h>


#define N 128
#define SCALE 2

#define TICKS 60

#define IX(x, y) ((x) + ((y)*N))

class fluidsim : public ui::view {
  ck::ref<ck::timer> tick_timer;

  float dt;    // time step
  float diff;  // diffusion
  float visc;  // viscocity

  float density[N * N];
  float s[N * N];

  float Vx0[N * N];
  float Vy0[N * N];

  float Vx[N * N];
  float Vy[N * N];

  float T = 0;

 public:
  fluidsim(float dt, float diffusion, float viscocity) : dt(dt), diff(diffusion), visc(viscocity) {
    set_flex_grow(1);
    tick_timer = ck::timer::make_interval(1000 / TICKS, [this] {
      float dx = cos(T) * 10;
      float dy = sin(T) * 10;
      T += 0.1;
      add_velocity(N / 2, N / 2, dx, dy);
      add_density(N / 2, N / 2, 10);

      this->step();
      repaint();
    });
  }

  ~fluidsim(void) {
  }


  void set_bnd(int b, float* x) {
    for (int i = 1; i < N - 1; i++) {
      x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
      x[IX(i, N - 1)] = b == 2 ? -x[IX(i, N - 2)] : x[IX(i, N - 2)];
    }
    for (int j = 1; j < N - 1; j++) {
      x[IX(0, j)] = b == 1 ? -x[IX(1, j)] : x[IX(1, j)];
      x[IX(N - 1, j)] = b == 1 ? -x[IX(N - 2, j)] : x[IX(N - 2, j)];
    }

    x[IX(0, 0)] = 0.5 * (x[IX(1, 0)] + x[IX(0, 1)]);
    x[IX(0, N - 1)] = 0.5 * (x[IX(1, N - 1)] + x[IX(0, N - 2)]);
    x[IX(N - 1, 0)] = 0.5 * (x[IX(N - 2, 0)] + x[IX(N - 1, 1)]);
    x[IX(N - 1, N - 1)] = 0.5 * (x[IX(N - 2, N - 1)] + x[IX(N - 1, N - 2)]);
  }


  void lin_solve(int b, float* x, float* x0, float a, float c, int iter) {
    float cRecip = 1.0 / c;
    for (int t = 0; t < iter; t++) {
      for (int j = 1; j < N - 1; j++) {
        for (int i = 1; i < N - 1; i++) {
          x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i + 1, j)] + x[IX(i - 1, j)] + x[IX(i, j + 1)] +
                                             x[IX(i, j - 1)])) *
                        cRecip;
        }
      }
      set_bnd(b, x);
    }
  }

  void diffuse(int b, float* x, float* x0, float diff, float dt, int iter) {
    float a = dt * diff * (N - 2) * (N - 2);
    lin_solve(b, x, x0, a, 1 + 6 * a, iter);
  }

  void project(float* velocX, float* velocY, float* p, float* div, int iter) {
    for (int j = 1; j < N - 1; j++) {
      for (int i = 1; i < N - 1; i++) {
        div[IX(i, j)] = (-0.5 * (velocX[IX(i + 1, j)] - velocX[IX(i - 1, j)] +
                                 velocY[IX(i, j + 1)] - velocY[IX(i, j - 1)])) /
                        N;
        p[IX(i, j)] = 0;
      }
    }

    set_bnd(0, div);
    set_bnd(0, p);
    lin_solve(0, p, div, 1, 6, iter);

    for (int j = 1; j < N - 1; j++) {
      for (int i = 1; i < N - 1; i++) {
        velocX[IX(i, j)] -= 0.5 * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * N;
        velocY[IX(i, j)] -= 0.5 * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * N;
      }
    }

    set_bnd(1, velocX);
    set_bnd(2, velocY);
  }

  void advect(int b, float* d, float* d0, float* velocX, float* velocY, float dt) {
    float i0, i1, j0, j1;

    float dtx = dt * (N - 2);
    float dty = dt * (N - 2);

    float s0, s1, t0, t1;
    float tmp1, tmp2, tmp3, x, y;

    float Nfloat = N - 2;
    float ifloat, jfloat;
    int i, j, k;

    for (j = 1, jfloat = 1; j < N - 1; j++, jfloat++) {
      for (i = 1, ifloat = 1; i < N - 1; i++, ifloat++) {
        tmp1 = dtx * velocX[IX(i, j)];
        tmp2 = dty * velocY[IX(i, j)];
        x = ifloat - tmp1;
        y = jfloat - tmp2;

        if (x < 0.5) x = 0.5;
        if (x > Nfloat + 0.5) x = Nfloat + 0.5;
        i0 = floor(x);
        i1 = i0 + 1.0;
        if (y < 0.5) y = 0.5;
        if (y > Nfloat + 0.5) y = Nfloat + 0.5;
        j0 = floor(y);
        j1 = j0 + 1.0;

        s1 = x - i0;
        s0 = 1.0 - s1;
        t1 = y - j0;
        t0 = 1.0 - t1;

        int i0i = i0;
        int i1i = i1;
        int j0i = j0;
        int j1i = j1;

        d[IX(i, j)] = s0 * (t0 * d0[IX(i0i, j0i)] + t1 * d0[IX(i0i, j1i)]) +
                      s1 * (t0 * d0[IX(i1i, j0i)] + t1 * d0[IX(i1i, j1i)]);
      }
    }

    set_bnd(b, d);
  }


  void step(void) {
    auto start = sysbind_gettime_microsecond();
    diffuse(1, Vx0, Vx, visc, dt, 4);
    diffuse(2, Vy0, Vy, visc, dt, 4);

    project(Vx0, Vy0, Vx, Vy, 4);

    advect(1, Vx, Vx0, Vx0, Vy0, dt);
    advect(2, Vy, Vy0, Vx0, Vy0, dt);

    project(Vx, Vy, Vx0, Vy0, 4);
    diffuse(0, s, density, diff, dt, 4);
    advect(0, density, s, Vx, Vy, dt);
  }

  void add_density(int x, int y, float amt) {
    if (x < 0 || x > N) return;
    if (y < 0 || y > N) return;
    if (density[IX(x, y)] <= 0 && amt < 0) return;
    density[IX(x, y)] += amt;
  }


  void add_velocity(int x, int y, float dx, float dy) {
    if (x < 0 || x > N) return;
    if (y < 0 || y > N) return;
    Vx[IX(x, y)] += dx;
    Vy[IX(x, y)] += dy;
  }

  virtual void paint_event(void) override {
    auto scribe = get_scribe();
    bool is_running = tick_timer->running();

    // draw all the cells
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        // int color = is_running ? 0xFFFFFF : 0x777777;
        int color = 0x000000;
        float d = density[IX(x, y)];
        if (d < 0) d = 0;
        if (d >= 1) d = 1;
        int gray = (255 * d);
        if (gray > 255) gray = 0xFF;
        color |= gray;
        color |= gray << 8;
        color |= gray << 16;

        for (int ox = 0; ox < SCALE; ox++) {
          for (int oy = 0; oy < SCALE; oy++) {
            scribe.draw_pixel(x * SCALE + ox, y * SCALE + oy, color);
          }
        }
      }
    }
  }


  int set_to = 1;
  virtual void mouse_event(ui::mouse_event& ev) override {
    auto p = ev.pos();
    if (within_self(p)) {
      if (ev.left) {
        auto pos = ev.pos();
        add_density(pos.x() / SCALE, pos.y() / SCALE, 1);
        add_velocity(pos.x() / SCALE, pos.y() / SCALE, ev.dx, ev.dy);
      }


      if (ev.right) {
        auto pos = ev.pos();
        add_density(pos.x() / SCALE, pos.y() / SCALE, -5);
        add_velocity(pos.x() / SCALE, pos.y() / SCALE, ev.dx, ev.dy);
      }
    }
  }
};




int main() {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("Fluid Simulation", N * SCALE, N * SCALE);
  // win->defer_invalidation(false);
  // win->compositor_sync(false);

  win->set_view<fluidsim>(0.2, 0, 0.00000001);

  // start the application!
  app.start();

  return 0;
}
