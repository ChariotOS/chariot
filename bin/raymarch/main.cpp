#include <ck/timer.h>
#include <gfx/font.h>
#include <ui/application.h>
#include <gfx/matrix.h>
#include <gfx/vector3.h>
#include <ck/time.h>
#include <gfx/color.h>
#include <math.h>

#define TICKS 30


#define MAX_STEPS 100
#define MAX_DIST 100.0f
#define SURF_DIST .01

class cubeview : public ui::view {
  ck::ref<ck::timer> tick_timer;
  ck::time::tracker gtime;

  float power = 5.0;


 public:
  cubeview() {
    tick_timer = ck::timer::make_interval(1000 / TICKS, [this] { this->update(); });
  }
  ~cubeview(void) {}


  float get_dist(gfx::float3 p) {
    float d = MAX_DIST;

#if 1
    // mandelbulb distance function
    gfx::float3 z = gfx::float3(p.x(), p.y(), p.z());
    float dr = 1;
    float r;

    for (int i = 0; i < 5; i++) {
      r = z.length();
      if (r > 2) {
        break;
      }

      float theta = acos(z.z() / r) * power;
      float phi = atan2(z.y(), z.x()) * power;

      float zr = pow(r, power);
      dr = pow(r, power - 1) * power * dr + 1;

      z = gfx::float3(sin(theta) * cos(phi), sin(phi) * sin(theta), cos(theta)) * zr;
      z += p;
    }


    float fract_dist = 0.5 * log(r) * r / dr;
    d = MIN(d, fract_dist);

#endif


    // printf("fract_dist = %f\n", fract_dist);
    // gfx::float3 s = {0, 1, 0};
    // float sphereDist = (p - s).length() - 0.2f;
    // d = MIN(d, sphereDist);

    // float planeDist = p.y() + 6;
    // d = MIN(d, planeDist);

    return d;
  }


  inline float raymarch(const gfx::float3 ro, const gfx::float3& rd) {
    float dO = 0.;

    for (int i = 0; i < MAX_STEPS; i++) {
      gfx::float3 p = ro + rd * dO;
      float dS = get_dist(p);
      dO += dS;
      if (dO > MAX_DIST || dS < SURF_DIST) break;
    }

    return dO;
  }


  gfx::float3 get_normal(gfx::float3 p) {
    float d = get_dist(p);
    float x = 0.01;
    float y = 0.0;

    gfx::float3 n = gfx::float3(d, d, d) - gfx::float3(get_dist(p - gfx::float3(x, y, y)),
                                               get_dist(p - gfx::float3(y, x, y)),
                                               get_dist(p - gfx::float3(y, y, x)));

    return n.normalized();
  }
  float get_light(gfx::float3 p, float itime) {
    gfx::float3 lightPos = {0.0, 1.0, -4.0};
    // lightPos.set_x(lightPos.x() + sin(itime) * 2.0);
    // lightPos.set_z(lightPos.z() + cos(itime) * 2.0);
    gfx::float3 l = (lightPos - p).normalized();
    gfx::float3 n = get_normal(p);

    float dif = CLAMP(n.dot(l), 0.0, 1.0);
    float d = raymarch(p + n * SURF_DIST * 2., l);
    if (d < (lightPos - p).length()) dif *= .1;

    return dif;
  }


  virtual void paint_event(gfx::scribe& s) override {
    ck::time::tracker timer;

    int ox = s.state().offset.x();
    int oy = s.state().offset.y();

    uint32_t* dst = s.bmp.scanline(oy) + ox;
    int stride = s.bmp.width();


    float invh = 1 / (float)height();
    float invw = 1 / (float)width();


    float itime = gtime.ms() / 1000.0;
    power = 10.0f;

    for (int y = 0; y < height(); y++) {
      float fy = -((y * invh) - 0.5);
      for (int x = 0; x < width(); x++) {
        float fx = -((x * invw) - 0.5);
        gfx::float3 uv = {fx, fy, 1.0f};    /// -0.5,-0.5 ... 0.5,0.5 location on the screen
        gfx::float3 ro = {0.0, 0.0, -4.0};  /// ray origin
        gfx::float3 rd = gfx::float3(uv.x(), uv.y(), 1.0f).normalized();  /// ray direction

        float d = raymarch(ro, rd);
        gfx::float3 p = ro + rd * d;

        float dif = get_light(p, itime);

        gfx::float3 col = gfx::float3(dif, dif, dif);
        col.clamp(-1.0, 1.0);
        col = col.pow(0.4545);

        // finally, get a uint32_t from the color we calculated
        uint32_t c = gfx::color::rgb(fabs(col.x()) * 255, fabs(col.y()) * 255, fabs(col.z()) * 255);
        dst[x] = c;
      }

      dst += stride;
    }

    printf("render took %llums\n", timer.ms());
  }
};




int main() {
  // connect to the window server
  ui::application app;

  auto win = ui::simple_window<cubeview>("Ray March Test", 200, 200);
  win.set_double_buffer(true);

  // start the application!
  app.start();

  return 0;
}
