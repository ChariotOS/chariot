#include <ck/timer.h>
#include <gfx/font.h>
#include <ui/application.h>
#include <sys/sysbind.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ck/time.h>

#define N 256
#define SCALE 3

#define TICKS 60

#define IX(x, y) ((x) + ((y)*N))

typedef struct RgbColor {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} RgbColor;

typedef struct HsvColor {
  unsigned char h;
  unsigned char s;
  unsigned char v;
} HsvColor;

RgbColor HsvToRgb(HsvColor hsv) {
  RgbColor rgb;
  unsigned char region, remainder, p, q, t;

  if (hsv.s == 0) {
    rgb.r = hsv.v;
    rgb.g = hsv.v;
    rgb.b = hsv.v;
    return rgb;
  }

  region = hsv.h / 43;
  remainder = (hsv.h - (region * 43)) * 6;

  p = (hsv.v * (255 - hsv.s)) >> 8;
  q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
  t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0:
      rgb.r = hsv.v;
      rgb.g = t;
      rgb.b = p;
      break;
    case 1:
      rgb.r = q;
      rgb.g = hsv.v;
      rgb.b = p;
      break;
    case 2:
      rgb.r = p;
      rgb.g = hsv.v;
      rgb.b = t;
      break;
    case 3:
      rgb.r = p;
      rgb.g = q;
      rgb.b = hsv.v;
      break;
    case 4:
      rgb.r = t;
      rgb.g = p;
      rgb.b = hsv.v;
      break;
    default:
      rgb.r = hsv.v;
      rgb.g = p;
      rgb.b = q;
      break;
  }

  return rgb;
}

HsvColor RgbToHsv(RgbColor rgb) {
  HsvColor hsv;
  unsigned char rgbMin, rgbMax;

  rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
  rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

  hsv.v = rgbMax;
  if (hsv.v == 0) {
    hsv.h = 0;
    hsv.s = 0;
    return hsv;
  }

  hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
  if (hsv.s == 0) {
    hsv.h = 0;
    return hsv;
  }

  if (rgbMax == rgb.r)
    hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
  else if (rgbMax == rgb.g)
    hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
  else
    hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

  return hsv;
}

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


  ck::ref<gfx::font> m_font;
  // mouse x and y
  int mx = 0, my = 0;

 public:
  fluidsim() : dt(0.2), diff(0), visc(0.0000000001) {
    m_font = gfx::font::get_default();
    tick_timer = ck::timer::make_interval(1000 / TICKS, [this] {
    // ck::time::logger l("fluidsim tick");
#if 1
      float dx = cos(T) * 10;
      float dy = sin(T) * 10;
      T += 0.1;
      add_velocity(N / 2, N / 2, dx, dy);
      add_density(N / 2, N / 2, 10);
#endif
      this->step();
    });
  }

  ~fluidsim(void) {}


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

    update();
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

  virtual void paint_event(gfx::scribe& scribe) override {
    bool is_running = tick_timer->running();

    scribe.clear(0x000000);
    // draw all the cells
#if 1
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        float d = density[IX(x, y)];

        int h = d * 255;
        h %= 255;

        HsvColor hsv;
        hsv.h = h;
        hsv.v = 0xFF;
        hsv.s = 0xFF;
        auto c = HsvToRgb(hsv);
        uint32_t color = (c.r << 16) | (c.g << 8) | (c.b);
        for (int ox = 0; ox < SCALE; ox++) {
          for (int oy = 0; oy < SCALE; oy++) {
            scribe.draw_pixel(x * SCALE + ox, y * SCALE + oy, color);
          }
        }
      }
    }
#endif


#if 0
    // draw vector lines
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        float vx = Vx[IX(x, y)] * 10.0;
        float vy = Vy[IX(x, y)] * 10.0;

        gfx::point root(x * SCALE + SCALE / 2, y * SCALE + SCALE / 2);
        float d = sqrt(vx * vx + vy * vy);  // density[IX(x, y)];
        int h = d * 255;
        h %= 255;

        HsvColor hsv;
        hsv.h = h;
        hsv.v = 0xFF;
        hsv.s = 0xFF;
        auto c = HsvToRgb(hsv);
        uint32_t color = (c.r << 16) | (c.g << 8) | (c.b);

        scribe.draw_line_antialias(root, root.translated(vx * SCALE, vy * SCALE), color);
      }
    }
#endif

    scribe.stackblur(SCALE, rect());
    // scribe.noise(0.05, rect());



    if (mx >= 0 && mx < N) {
      if (my >= 0 && my < N) {
        m_font->with_line_height(12, [&]() {
          float d = density[IX(mx, my)];

          float vx = Vx[IX(mx, my)];
          float vy = Vy[IX(mx, my)];
          char buf[64];
          snprintf(buf, sizeof(buf), "%3d,%3d: d:%g, v:%g,%g", mx, my, d, vx, vy);
          scribe.draw_text(*m_font, gfx::rect(0, 0, width(), 12), buf, ui::TextAlign::CenterLeft,
              0xFFFFFF, true);
        });
      }
    }
  }


  virtual void mouse_event(ui::mouse_event& ev) override {
    auto p = ev.pos();
    mx = p.x() / SCALE;
    my = p.y() / SCALE;
    if (within_self(p)) {
      if (ev.left) {
        auto pos = ev.pos();
        add_density(pos.x() / SCALE, pos.y() / SCALE, 10);
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
  ui::application app;

  auto win = ui::simple_window<fluidsim>("Fluid Simluation", N * SCALE, N * SCALE);
  win.set_double_buffer(true);

  // start the application!
  app.start();

  return 0;
}
