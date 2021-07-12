#define PL_MPEG_IMPLEMENTATION
#include <gfx/color.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "pl_mpeg.h"

#include <ck/timer.h>
#include <gfx/bitmap.h>
#include <ui/application.h>
#include <ui/view.h>

struct mpegview : public ui::view {
  plm_t *plm;
  ck::ref<ck::timer> timer;

  double last_time = 0;

  ck::ref<gfx::bitmap> backing_bitmap = nullptr;
  static constexpr int FPS = 30;

 public:
  mpegview() {
    last_time = (double)clock() / 1000.0;
    plm = plm_create_with_filename("/usr/res/misc/bunny.mpg");

    // nice lol
    plm_set_video_decode_callback(
        plm,
        [](plm_t *mpeg, plm_frame_t *frame, void *user) {
          ((mpegview *)user)->handle_frame(frame);
        },
        (void *)this);

    plm_set_audio_decode_callback(
        plm, [](plm_t *mpeg, plm_samples_t *samples, void *user) {}, (void *)this);

    /* Create a 30fps timer */
    timer = ck::timer::make_interval(1000 / FPS, [this]() { this->tick(); });
  }

  virtual ~mpegview(void) { plm_destroy(plm); }

  void tick(void) {
    if (!plm_has_ended(plm)) {
      double current_time = (double)clock() / 1000.0;
      double elapsed_time = current_time - last_time;
      /*
if (elapsed_time > 1.0 / (float)FPS) {
elapsed_time = 1.0 / (float)FPS;
}
      */
      last_time = current_time;
      // printf("delta: %f seconds\n", elapsed_time);
      plm_decode(plm, elapsed_time);
    } else {
      timer->stop();
    }
  }



  void handle_frame(plm_frame_t *frame) {
    if (backing_bitmap.is_null() || backing_bitmap->width() != frame->width ||
        backing_bitmap->height() != frame->height) {
      printf("allocate a new %dx%d bitmap\n", frame->width, frame->height);
      backing_bitmap = new gfx::bitmap(frame->width, frame->height);
    }

    auto *win = surface();

    if (win != NULL && (width() != frame->width || height() != frame->height)) {
      win->resize(frame->width, frame->height);
    }
    plm_frame_to_bgra(frame, (uint8_t *)backing_bitmap->pixels(), backing_bitmap->width() * 4);

    update();
  }



  virtual void mouse_event(ui::mouse_event &ev) override {
    float seek_to = plm_get_duration(plm) * ((float)ev.pos().x() / (float)width());
    plm_seek(plm, seek_to, 1);
    if (!timer->running()) timer->start(1000 / FPS, true);
  }

  virtual void paint_event(gfx::scribe &s) override {
    if (backing_bitmap.is_null()) return;

    gfx::rect r(0, 0, width(), height());

    s.blit(gfx::point(0, 0), *backing_bitmap, backing_bitmap->rect());
  }
};


int main(int argc, char **argv) {
  ui::application app;

  ui::window *win = new ui::simple_window<mpegview>("MPEG Viewer", 640, 360);

  app.start();

  return 0;
}
