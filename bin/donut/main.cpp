#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main() {
  float A = 0, B = 0;
  float i, j;
  int k;
  float z[1760];
  char b[1760];
  printf("\x1b[2J");
  for (;;) {
    memset(b, 32, 1760);
    memset(z, 0, 7040);
    for (j = 0; j < 6.28; j += 0.07) {
      for (i = 0; i < 6.28; i += 0.02) {
        float c = sin(i);
        float d = cos(j);
        float e = sin(A);
        float f = sin(j);
        float g = cos(A);
        float h = d + 2;
        float D = 1 / (c * h * e + f * g + 5);
        float l = cos(i);
        float m = cos(B);
        float n = sin(B);
        float t = c * h * g - f * e;
        int x = 40 + 30 * D * (l * h * m - t * n);
        int y = 12 + 15 * D * (l * h * n + t * m);
        int o = x + 80 * y;
        int N = 8 * ((f * e - c * d * g) * m - c * d * e - f * g - l * d * n);
        if (22 > y && y > 0 && x > 0 && 80 > x && D > z[o]) {
          z[o] = D;
          b[o] = ".,-~:;=!*#$@"[N > 0 ? N : 0];
        }
      }
    }
    printf("\x1b[H");
    for (k = 0; k < 1761; k++) {
      putchar(k % 80 ? b[k] : 10);
      A += 0.00004;
      B += 0.00002;
    }
    usleep(30000);
  }
  return 0;
}



#if 0
struct donutview : public ui::view {
 public:
  donutview() {
  }



  void draw_char(char c, int x, int y, uint32_t fg, uint32_t bg) {
    auto s = get_scribe();
  }


  virtual void paint_event(void) {
    auto s = get_scribe();

    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        draw_char((r + c) % ('z' - 'a') + 'a', c, r, ui::Color::White, ui::Color::Black);
      }
    }
  }
};

int main() {
  ui::application app;

  ui::window *win = app.new_window("Terminal", 30, 30);

  int cols = 80;
  int rows = 24;
  win->set_view<terminalview>(rows, cols);

  app.start();
	return 0;
}
#endif
