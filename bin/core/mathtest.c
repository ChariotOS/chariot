#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


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




void do_graph(const char *name, double (*f)(double), float lo, float hi) {
  float d = (hi - lo) / (255 / 4.0);

  double min_fx = DBL_MAX;
  double max_fx = DBL_MIN;
  for (float x = lo; x < hi; x += d) {
    double fx = f(x);
    if (fx > max_fx) max_fx = fx;
    if (fx < min_fx) min_fx = fx;
  }

  int width = 120;

  double norm_dnom = (max_fx - min_fx);

  for (float x = lo; x < hi; x += d) {
    double fx = f(x);
    float norm = (fx - min_fx) / norm_dnom;
    int px = width * norm;
    printf("%s(%8.3f) = %8.3f    |", name, x, fx);


    HsvColor hsv;
    hsv.h = 256 * ((x - lo) / (hi - lo));
    hsv.s = 128;
    hsv.v = 0xFF;

    RgbColor rgb = HsvToRgb(hsv);

    // printf("\x1b[4%dm", color);
    printf("\x1b[48;2;%d;%d;%dm", rgb.r, rgb.g, rgb.b);
    for (int i = 0; i < px; i++)
      printf(" ");
    printf("\x1b[0m \t%.2f\n", fx);
    usleep(1000 * 3);
  }
  printf("\n");
}

double square(double x) {
  return pow(x, 2);
}

int main() {
  do_graph("sin", square, -M_PI, M_PI);
  do_graph("cos", cos, -M_PI, M_PI);
  do_graph("sqrt", sqrt, 0, 400);
  do_graph("exp", exp, 0, 2);
  do_graph("square", square, -10, 10);
  do_graph("abs", fabs, -10, 10);

  return 0;
}
