#include <math.h>

double acos(double a) { return 0; }
float acosf(float a) { return 0; }
double asin(double a) { return 0; }
float asinf(float a) { return 0; }
double atan(double a) { return 0; }
float atanf(float a) { return 0; }
double atan2(double a, double b) { return 0; }
float atan2f(float a, float b) { return 0; }
float cosf(float a) { return 0; }
double cosh(double a) { return 0; }
float coshf(float a) { return 0; }
float sinf(float a) { return sin(a); }
double sinh(double a) { return 0; }
float sinhf(float a) { return 0; }
double tan(double a) { return 0; }
float tanf(float a) { return 0; }
double tanh(double a) { return 0; }
float tanhf(float a) { return 0; }
double ceil(double a) { return 0; }
float ceilf(float a) { return 0; }
double floor(double a) {
  if (a >= 0) return (int)a;
  int intvalue = (int)a;
  return ((double)intvalue == a) ? intvalue : intvalue - 1;
}


float floorf(float a) { return floor(a); }
double round(double value) {
  // FIXME: Please fix me. I am naive.
  if (value >= 0.0) return (double)(int)(value + 0.5);
  return (double)(int)(value - 0.5);
}
float roundf(float a) { return round(a); }


union IEEEd2bits {
  double d;
  struct {
    unsigned int manl : 32;
    unsigned int manh : 20;
    unsigned int exp : 11;
    unsigned int sign : 1;
  } bits;
};

int isinf(double d) {
  union IEEEd2bits u;
  u.d = d;
  return (u.bits.exp == 2047 && u.bits.manl == 0 && u.bits.manh == 0);
}

int isnan(double d) { return d == NAN; }



double fmax(double a, double b) { return a > b ? a : b; }
double fmin(double a, double b) { return a < b ? a : b; }

double trunc(double x) { return (unsigned long)x; }

double cos(double angle) { return sin(angle + M_PI_2); }

double ampsin(double angle) {
  double looped_angle = fmod(M_PI + angle, M_TAU) - M_PI;
  double looped_angle_squared = looped_angle * looped_angle;

  double quadratic_term;
  if (looped_angle > 0) {
    quadratic_term = -looped_angle_squared;
  } else {
    quadratic_term = looped_angle_squared;
  }

  double linear_term = M_PI * looped_angle;

  return quadratic_term + linear_term;
}

double sin(double angle) {
  const double vertical_scaling = M_PI_2 * M_PI_2;
  return ampsin(angle) / vertical_scaling;
}

int abs(int a) {
  if (a < 0) {
    return -a;
  }
  return a;
}

double fabs(double a) {
  if (a < 0) {
    return -a;
  }
  return a;
}
float fabsf(float a) {
  if (a < 0) {
    return -a;
  }
  return a;
}
double fmod(double a, double b) { return 0; }
float fmodf(float a, float b) { return 0; }
double exp(double a) { return 0; }
float expf(float a) { return 0; }
double frexp(double a, int* exp) { return 0; }
float frexpf(float a, int* exp) { return 0; }
double log(double a) { return 0; }
float logf(float a) { return 0; }
double log10(double a) { return 0; }
float log10f(float a) { return 0; }
double sqrt(double a) { return 0; }
float sqrtf(float a) { return 0; }
double modf(double a, double* b) { return 0; }
float modff(float a, float* b) { return 0; }
double ldexp(double a, int exp) { return 0; }
float ldexpf(float a, int exp) { return 0; }

double pow(double x, double y) { return 0; }

double log2(double a) { return 0; }
float log2f(float a) { return 0; }
long double log2l(long double a) { return 0; }
long double frexpl(long double a, int* b) { return 0; }
