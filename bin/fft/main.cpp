#include <gfx/color.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <ck/timer.h>
#include <gfx/bitmap.h>
#include <ui/application.h>
#include <ui/view.h>
#include "./fft.h"
#include <sys/sysbind.h>

struct fftview : public ui::view {
 public:
  fftview() {
    set_flex_grow(1.0);
  }

  virtual ~fftview(void) {
  }

  virtual void paint_event(void) override {
    auto s = get_scribe();
    s.clear(0);

    // Construct input signal
    FFT_PRECISION sample_rate = width() * 2;  // signal sampling rate
    int n = width();                          // 3 seconds of data, n has to be greater than 1
    // FFT_PRECISION f = 2;              // frequency of the artifical signal
    FFT_PRECISION *input = (FFT_PRECISION *)malloc(n * sizeof(FFT_PRECISION));  // Input signal

    int top_center = height() / 2;
    int bottom = height();

    FFT_PRECISION highest_mag = 0;
    FFT_PRECISION prev_mag = 0;

    struct wave {
      double cos_comp;
      double sin_comp;
      double freq;
    };

    int wavec = 3;
    // some number of waves.
    struct wave waves[wavec];

    for (int i = 0; i < wavec; i++) {
      float mul = (float)i / (float)n;
      waves[i].cos_comp = fmod(rand(), (height() / 8.0)); //  * mul;
      waves[i].sin_comp = 0; // fmod(rand(), (height() / 8.0)) * mul;
      waves[i].freq = fmod(rand(), width());
    }

    for (int i = 0; i < n; i++) {
      input[i] = 0.0;
      for (int w = 0; w < wavec; w++) {
        input[i] += waves[w].sin_comp * sin(2 * M_PI * waves[w].freq * i / sample_rate);
        input[i] += waves[w].cos_comp * cos(2 * M_PI * waves[w].freq * i / sample_rate);
      }

      if (input[i] > highest_mag) highest_mag = input[i];
    }


    prev_mag = 0;
    for (int i = 0; i < n; i++) {
      float mag = input[i];
      mag /= highest_mag;
      mag *= float(height()) / 2;

      s.draw_line(gfx::point(i, top_center - mag), gfx::point(i, top_center + mag), 0x888888);
      prev_mag = mag;
    }


    // Initialize fourier transformer
    FFTTransformer *transformer = create_fft_transformer(n, FFT_SCALED_OUTPUT);

    auto start = sysbind_gettime_microsecond();
    // Transform signal
    fft_forward(transformer, input);
    auto end = sysbind_gettime_microsecond();

    printf("FFT took %llu us\n", end - start);

    prev_mag = 0;
    highest_mag = 0;
    for (int i = 0; i < n; i += 2) {
      FFT_PRECISION cos_comp = input[i];
      FFT_PRECISION sin_comp = input[i + 1];
      FFT_PRECISION mag = sqrt((cos_comp * cos_comp) + (sin_comp * sin_comp));
      if (mag > highest_mag) highest_mag = mag;
    }
    // Print output
    for (int i = 0; i < n; i += 2) {
      FFT_PRECISION cos_comp = input[i];
      FFT_PRECISION sin_comp = input[i + 1];
      FFT_PRECISION mag = sqrt((cos_comp * cos_comp) + (sin_comp * sin_comp));

      mag /= highest_mag;
      mag *= float(height()) / 2;
      s.draw_line(gfx::point(i - 2, bottom - prev_mag), gfx::point(i, bottom - mag), 0x00FF00);

      s.draw_line(gfx::point(i, bottom), gfx::point(i, bottom - mag), 0x00FF00);
      prev_mag = mag;
    }

    free_fft_transformer(transformer);
    free(input);
  }

  virtual void on_keydown(ui::keydown_event &ev) override {
    repaint(true);
  }
};


int main(int argc, char **argv) {
  ui::application app;

  ui::window *win = app.new_window("FFT", 640, 360);
  auto &v = win->set_view<fftview>();
  // win->compositor_sync(true);


  app.start();

  return 0;
}
