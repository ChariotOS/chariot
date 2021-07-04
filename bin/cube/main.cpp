#include <ck/timer.h>
#include <gfx/font.h>
#include <ui/application.h>
#include <gfx/matrix.h>
#include <gfx/vector3.h>
#include <ck/time.h>
#include <gfx/color.h>

#define TICKS 30



class cubeview : public ui::view {
  ck::ref<ck::timer> tick_timer;
  ck::time::tracker timer;


 public:
  cubeview() {
    tick_timer = ck::timer::make_interval(1000 / TICKS, [this] { this->tick(); });
  }
  ~cubeview(void) {}

  void tick(void) {
    update();

    m_accumulated_time += timer.ms();
    m_cycles++;
  }

  virtual void paint_event(gfx::scribe& s) override {
    const gfx::float3 vertices[8]{
        {-1, -1, -1},
        {-1, 1, -1},
        {1, 1, -1},
        {1, -1, -1},
        {-1, -1, 1},
        {-1, 1, 1},
        {1, 1, 1},
        {1, -1, 1},
    };

#define QUAD(a, b, c, d) a, b, c, c, d, a

    const int indices[]{
        QUAD(0, 1, 2, 3),
        QUAD(7, 6, 5, 4),
        QUAD(4, 5, 1, 0),
        QUAD(3, 2, 6, 7),
        QUAD(1, 5, 6, 2),
        QUAD(0, 3, 7, 4),
    };

    const uint32_t colors[]{
        0xFF0000,
        0xFF0000,
        0x00FF00,
        0x00FF00,
        0x0000FF,
        0x0000FF,
        0xFF00FF,
        0xFF00FF,
        0xFFFFFF,
        0xFFFFFF,
        0x00FFFF,
        0x00FFFF,
    };

    gfx::float3 transformed_vertices[8];

    float angle = timer.ms() / 1000.0;

    auto matrix = gfx::translation_matrix(gfx::float3(0, 0, 1.5f)) *
                  gfx::rotation_matrix(gfx::float3(1, 0, 0), angle * 1.17356641f) *
                  gfx::rotation_matrix(gfx::float3(0, 1, 0), angle * 0.90533273f) *
                  gfx::rotation_matrix(gfx::float3(0, 0, 1), angle);

    for (int i = 0; i < 8; i++) {
      transformed_vertices[i] = transform_point(matrix, vertices[i]);
    }

    s.clear(0x999999);

    auto to_point = [](const gfx::float3& v) { return gfx::point(v.x(), v.y()); };

    float w = width();
    float h = height();

    for (size_t i = 0; i < sizeof(indices) / sizeof(indices[0]) / 3; i++) {
      auto a = transformed_vertices[indices[i * 3]];
      auto b = transformed_vertices[indices[i * 3 + 1]];
      auto c = transformed_vertices[indices[i * 3 + 2]];
      auto normal = (b - a).cross(c - a);
      normal.normalize();

      // Perspective projection
      a.set_x(w / 2 + a.x() / (1 + a.z() * 0.35f) * w / 3);
      a.set_y(h / 2 - a.y() / (1 + a.z() * 0.35f) * w / 3);
      b.set_x(w / 2 + b.x() / (1 + b.z() * 0.35f) * w / 3);
      b.set_y(h / 2 - b.y() / (1 + b.z() * 0.35f) * w / 3);
      c.set_x(w / 2 + c.x() / (1 + c.z() * 0.35f) * w / 3);
      c.set_y(h / 2 - c.y() / (1 + c.z() * 0.35f) * w / 3);

      float winding = (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
      if (winding < 0) continue;

      float shade = 0.5f + normal.y() * 0.5f;
      auto color = colors[i];
      int cr, cg, cb;

      cr = ((color >> 16) & 0xFF) * shade;
      cg = ((color >> 8) & 0xFF) * shade;
      cb = ((color >> 0) & 0xFF) * shade;


      s.draw_triangle(to_point(a), to_point(b), to_point(c), gfx::color::rgb(cr, cg, cb));
    }
  }

 private:
  int m_accumulated_time;
  int m_cycles;
  int m_phase;
};




int main() {
  // connect to the window server
  ui::application app;

  auto win = ui::simple_window<cubeview>("Ray March Test", 250, 250);

  // start the application!
  app.start();

  return 0;
}
