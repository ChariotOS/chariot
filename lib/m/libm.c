#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#define MATH

#include "internal.h"

#if 0
#undef MATH
#define MATH                                                                          \
  do {                                                                                \
    if (getpid() != 6) {                                                              \
      fprintf(stderr, "[LIBM pid %d] called math function %s\n", getpid(), __func__); \
    }                                                                                 \
  } while (0)

#endif

#define BAD                                                              \
  do {                                                                   \
    if (1) {                                                             \
      fprintf(stderr, "[LIBM] called bad math function %s\n", __func__); \
    }                                                                    \
  } while (0)


double acos(double x) {
  MATH;
  float negate = (float)(x < 0);
  x = abs(x);
  float ret = -0.0187293;
  ret = ret * x;
  ret = ret + 0.0742610;
  ret = ret * x;
  ret = ret - 0.2121144;
  ret = ret * x;
  ret = ret + 1.5707288;
  ret = ret * sqrt(1.0 - x);
  ret = ret - 2 * negate * ret;
  return negate * 3.14159265358979 + ret;
}
float acosf(float a) {
  return acos(a);
}



double asin(double a) {
  BAD;
  return 0;
}
float asinf(float a) {
  BAD;
  return 0;
}
double atan(double a) {
  BAD;
  return 0;
}
float atanf(float a) {
  BAD;
  return 0;
}
double atan2(double a, double b) {
  BAD;
  return 0;
}
float atan2f(float a, float b) {
  BAD;
  return 0;
}


float cosf(float a) {
  return cos(a);
}
double cosh(double x) {
  return log(x + sqrt(x * x - 1));
}
float coshf(float x) {
  return log(x + sqrt(x * x - 1));
}
float sinf(float a) {
  return sin(a);
}

double sinh(double x) {
  MATH;
  double exponentiated = exp(x);
  if (x > 0) return (exponentiated * exponentiated - 1) / 2 / exponentiated;
  return (exponentiated - 1 / exponentiated) / 2;
}
float sinhf(float x) {
  MATH;
  double exponentiated = expf(x);
  if (x > 0) return (exponentiated * exponentiated - 1) / 2 / exponentiated;
  return (exponentiated - 1 / exponentiated) / 2;
}

// bad, slow definition but I don't really think it matters.
double tan(double a) {
  return sin(a) / cos(a);
}
float tanf(float a) {
  return tan(a);
}


double tanh(double a) {
  BAD;
  return 0;
}
float tanhf(float a) {
  BAD;
  return 0;
}
double ceil(double x) {
  MATH;
  int as_int = (int)x;
  if (x == (float)as_int) return as_int;
  if (x < 0) {
    if (as_int == 0) return -0;
    return as_int;
  }
  return as_int + 1;
}


float ceilf(float a) {
  return ceil(a);
}



double floor(double a) {
  MATH;
  if (a >= 0) return (int)a;
  int intvalue = (int)a;
  return ((double)intvalue == a) ? intvalue : intvalue - 1;
}


float floorf(float a) {
  return floor(a);
}
double round(double value) {
  MATH;
  // FIXME: Please fix me. I am naive.
  if (value >= 0.0) return (double)(int)(value + 0.5);
  return (double)(int)(value - 0.5);
}
float roundf(float a) {
  return round(a);
}


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
  MATH;
  union IEEEd2bits u;
  u.d = d;
  return (u.bits.exp == 2047 && u.bits.manl == 0 && u.bits.manh == 0);
}

int isnan(double d) {
  return d != d;
}



double fmax(double a, double b) {
  return a > b ? a : b;
}
double fmin(double a, double b) {
  return a < b ? a : b;
}

double trunc(double x) {
  return (unsigned long)x;
}
float truncf(float x) {
  return (unsigned long)x;
}



static const double S1 = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
    S2 = 8.33333333332248946124e-03,                  /* 0x3F811111, 0x1110F8A6 */
    S3 = -1.98412698298579493134e-04,                 /* 0xBF2A01A0, 0x19C161D5 */
    S4 = 2.75573137070700676789e-06,                  /* 0x3EC71DE3, 0x57B1FE7D */
    S5 = -2.50507602534068634195e-08,                 /* 0xBE5AE5E6, 0x8A2B9CEB */
    S6 = 1.58969099521155010221e-10;                  /* 0x3DE5D93A, 0x5ACFD57C */

double __sin(double x, double y, int iy) {
  double z, r, v, w;
  z = x * x;
  w = z * z;
  r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
  v = z * x;
  if (iy == 0)
    return x + v * (S1 + z * r);
  else
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}


static const double C1 = 4.16666666666666019037e-02, /* 0x3FA55555, 0x5555554C */
    C2 = -1.38888888888741095749e-03,                /* 0xBF56C16C, 0x16C15177 */
    C3 = 2.48015872894767294178e-05,                 /* 0x3EFA01A0, 0x19CB1590 */
    C4 = -2.75573143513906633035e-07,                /* 0xBE927E4F, 0x809C52AD */
    C5 = 2.08757232129817482790e-09,                 /* 0x3E21EE9E, 0xBDB4B1C4 */
    C6 = -1.13596475577881948265e-11;                /* 0xBDA8FAE9, 0xBE8838D4 */

double __cos(double x, double y) {
  double hz, z, r, w;

  z = x * x;
  w = z * z;
  r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
  hz = 0.5 * z;
  w = 1.0 - hz;
  return w + (((1.0 - w) - hz) + (z * r - x * y));
}




extern int __rem_pio2(double x, double* y);


#define GET_HIGH_WORD(hi, x)       \
  do {                             \
    (hi) = (*(uint64_t*)&x) >> 32; \
  } while (0)

double sin(double x) {
  double y[2];
  uint32_t ix;
  unsigned n;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);
  ix &= 0x7fffffff;

  /* |x| ~< pi/4 */
  if (ix <= 0x3fe921fb) {
    if (ix < 0x3e500000) { /* |x| < 2**-26 */
      /* raise inexact if x != 0 and underflow if subnormal*/
      // FORCE_EVAL(ix < 0x00100000 ? x / 0x1p120f : x + 0x1p120f);
      return x;
    }
    return __sin(x, 0.0, 0);
  }

  /* sin(Inf or NaN) is NaN */
  if (ix >= 0x7ff00000) return x - x;

  /* argument reduction needed */
  n = __rem_pio2(x, y);
  switch (n & 3) {
    case 0:
      return __sin(y[0], y[1], 1);
    case 1:
      return __cos(y[0], y[1]);
    case 2:
      return -__sin(y[0], y[1], 1);
    default:
      return -__cos(y[0], y[1]);
  }
}


double cos(double x) {
  double y[2];
  uint32_t ix;
  unsigned n;

  GET_HIGH_WORD(ix, x);
  ix &= 0x7fffffff;

  /* |x| ~< pi/4 */
  if (ix <= 0x3fe921fb) {
    if (ix < 0x3e46a09e) { /* |x| < 2**-27 * sqrt(2) */
      /* raise inexact if x!=0 */
      (void)(x + 0x1p120f);
      return 1.0;
    }
    return __cos(x, 0);
  }

  /* cos(Inf or NaN) is NaN */
  if (ix >= 0x7ff00000) return x - x;

  /* argument reduction */
  n = __rem_pio2(x, y);
  switch (n & 3) {
    case 0:
      return __cos(y[0], y[1]);
    case 1:
      return -__sin(y[0], y[1], 1);
    case 2:
      return -__cos(y[0], y[1]);
    default:
      return __sin(y[0], y[1], 1);
  }
}



int abs(int a) {
  MATH;
  if (a < 0) {
    return -a;
  }
  return a;
}

double fabs(double a) {
  MATH;
  if (a < 0) {
    return -a;
  }
  return a;
}
float fabsf(float a) {
  MATH;
  if (a < 0) {
    return -a;
  }
  return a;
}
double fmod(double index, double period) {
  MATH;
  return index - trunc(index / period) * period;
}

float fmodf(float index, float period) {
  MATH;
  return index - trunc(index / period) * period;
}


double exp(double a) {
  MATH;
  return pow(M_E, a);
}
float expf(float a) {
  MATH;
  return pow(M_E, a);
}

double frexp(double a, int* exp) {
  MATH;
  BAD;
  return 0;
}
float frexpf(float a, int* exp) {
  MATH;
  BAD;
  return 0;
}
double log(double a) {
  MATH;
  BAD;
  return 0;
}
float logf(float a) {
  MATH;
  BAD;
  return 0;
}
double log10(double a) {
  MATH;
  BAD;
  return 0;
}
float log10f(float a) {
  MATH;
  BAD;
  return 0;
}



/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) {
  return (uint64_t)a * b >> 32;
}

/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
  uint64_t ahi = a >> 32;
  uint64_t alo = a & 0xffffffff;
  uint64_t bhi = b >> 32;
  uint64_t blo = b & 0xffffffff;
  return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}


const uint16_t __rsqrt_tab[128] = {
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43, 0xaa14, 0xa8eb, 0xa7c8, 0xa6aa,
    0xa592, 0xa480, 0xa373, 0xa26b, 0xa168, 0xa06a, 0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1,
    0x99f0, 0x9913, 0x983a, 0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430, 0x936b, 0x92a9, 0x91ea, 0x912e,
    0x9075, 0x8fbe, 0x8f0a, 0x8e59, 0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07, 0x8a64, 0x89c4, 0x8925,
    0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599, 0x8508, 0x8479, 0x83ec, 0x8361, 0x82d8, 0x8250,
    0x81c9, 0x8145, 0x80c2, 0x8040, 0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2, 0xe443, 0xe2dc, 0xe17a, 0xe020,
    0xdecb, 0xdd7d, 0xdc34, 0xdaf1, 0xd9b3, 0xd87b, 0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192,
    0xd07b, 0xcf69, 0xce5b, 0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f, 0xc858, 0xc764, 0xc674, 0xc587,
    0xc49d, 0xc3b7, 0xc2d4, 0xc1f4, 0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe, 0xbcef, 0xbc23, 0xbb59,
    0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0, 0xb617, 0xb560,
};
double sqrt(double x) {
  uint64_t ix, top, m;

  /* special case handling.  */
  ix = asuint64(x);
  top = ix >> 52;
  if (predict_false(top - 0x001 >= 0x7ff - 0x001)) {
    /* x < 0x1p-1022 or inf or nan.  */
    if (ix * 2 == 0) return x;
    if (ix == 0x7ff0000000000000) return x;
    if (ix > 0x7ff0000000000000) return __math_invalid(x);
    /* x is subnormal, normalize it.  */
    ix = asuint64(x * 0x1p52);
    top = ix >> 52;
    top -= 52;
  }

  /* argument reduction:
     x = 4^e m; with integer e, and m in [1, 4)
     m: fixed point representation [2.62]
     2^e is the exponent part of the result.  */
  int even = top & 1;
  m = (ix << 11) | 0x8000000000000000;
  if (even) m >>= 1;
  top = (top + 0x3ff) >> 1;

  /* approximate r ~ 1/sqrt(m) and s ~ sqrt(m) when m in [1,4)

     initial estimate:
     7bit table lookup (1bit exponent and 6bit significand).

     iterative approximation:
     using 2 goldschmidt iterations with 32bit int arithmetics
     and a final iteration with 64bit int arithmetics.

     details:

     the relative error (e = r0 sqrt(m)-1) of a linear estimate
     (r0 = a m + b) is |e| < 0.085955 ~ 0x1.6p-4 at best,
     a table lookup is faster and needs one less iteration
     6 bit lookup table (128b) gives |e| < 0x1.f9p-8
     7 bit lookup table (256b) gives |e| < 0x1.fdp-9
     for single and double prec 6bit is enough but for quad
     prec 7bit is needed (or modified iterations). to avoid
     one more iteration >=13bit table would be needed (16k).

     a newton-raphson iteration for r is
       w = r*r
       u = 3 - m*w
       r = r*u/2
     can use a goldschmidt iteration for s at the end or
       s = m*r

     first goldschmidt iteration is
       s = m*r
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     next goldschmidt iteration is
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     and at the end r is not computed only s.

     they use the same amount of operations and converge at the
     same quadratic rate, i.e. if
       r1 sqrt(m) - 1 = e, then
       r2 sqrt(m) - 1 = -3/2 e^2 - 1/2 e^3
     the advantage of goldschmidt is that the mul for s and r
     are independent (computed in parallel), however it is not
     "self synchronizing": it only uses the input m in the
     first iteration so rounding errors accumulate. at the end
     or when switching to larger precision arithmetics rounding
     errors dominate so the first iteration should be used.

     the fixed point representations are
       m: 2.30 r: 0.32, s: 2.30, d: 2.30, u: 2.30, three: 2.30
     and after switching to 64 bit
       m: 2.62 r: 0.64, s: 2.62, d: 2.62, u: 2.62, three: 2.62  */

  static const uint64_t three = 0xc0000000;
  uint64_t r, s, d, u, i;

  i = (ix >> 46) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r sqrt(m) - 1| < 0x1.fdp-9 */
  s = mul32(m >> 32, r);
  /* |s/sqrt(m) - 1| < 0x1.fdp-9 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
  r = r << 32;
  s = mul64(m, r);
  d = mul64(s, r);
  u = (three << 32) - d;
  s = mul64(s, u); /* repr: 3.61 */
  /* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
  s = (s - 2) >> 9; /* repr: 12.52 */
  /* -0x1.09p-52 < s - sqrt(m) < -0x1.fffcp-63 */

  /* s < sqrt(m) < s + 0x1.09p-52,
     compute nearest rounded result:
     the nearest result to 52 bits is either s or s+0x1p-52,
     we can decide by comparing (2^52 s + 0.5)^2 to 2^104 m.  */
  uint64_t d0, d1, d2;
  double y, t;
  d0 = (m << 42) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 63;
  s &= 0x000fffffffffffff;
  s |= top << 52;
  y = asdouble(s);
#if 0
  if (FENV_SUPPORT) {
    /* handle rounding modes and inexact exception:
       only (s+1)^2 == 2^42 m case is exact otherwise
       add a tiny value to cause the fenv effects.  */
    uint64_t tiny = predict_false(d2 == 0) ? 0 : 0x0010000000000000;
    tiny |= (d1 ^ d2) & 0x8000000000000000;
    t = asdouble(tiny);
    y = eval_as_double(y + t);
  }
#endif
  return y;
}




float sqrtf(float x) {
  return sqrt(x);
}
double modf(double a, double* b) {
  MATH;
  BAD;
  return 0;
}
float modff(float a, float* b) {
  MATH;
  BAD;
  return 0;
}
double ldexp(double a, int exp) {
  MATH;
  BAD;
  return 0;
}
float ldexpf(float a, int exp) {
  BAD;
  return 0;
}

float powf(float x, float y) {
  return pow(x, y);
}

double log2(double a) {
  BAD;
  return 0;
}
float log2f(float a) {
  BAD;
  return 0;
}
long double log2l(long double a) {
  BAD;
  return 0;
}
long double frexpl(long double a, int* b) {
  BAD;
  return 0;
}

double expm1(double x) {
  return pow(M_E, x) - 1;
}

double cbrt(double x) {
  if (x > 0) {
    return pow(x, 1.0 / 3.0);
  }

  return -pow(-x, 1.0 / 3.0);
}

double log1p(double x) {
  return log(1 + x);
}

double acosh(double x) {
  return log(x + sqrt(x * x - 1));
}

double asinh(double x) {
  return log(x + sqrt(x * x + 1));
}

double atanh(double x) {
  return log((1 + x) / (1 - x)) / 2.0;
}

double hypot(double x, double y) {
  return sqrt(x * x + y * y);
}

int fesetround(int x) {
  return 0;
}



/*
 * Floats whose exponent is in [1..INFNAN) (of whatever type) are
 * `normal'.  Floats whose exponent is INFNAN are either Inf or NaN.
 * Floats whose exponent is zero are either zero (iff all fraction
 * bits are zero) or subnormal values.
 *
 * A NaN is a `signalling NaN' if its QUIETNAN bit is clear in its
 * high fraction; if the bit is set, it is a `quiet NaN'.
 */
#define SNG_EXP_INFNAN 255
#define DBL_EXP_INFNAN 2047
#define EXT_EXP_INFNAN 32767
struct ieee_double {
  unsigned int dbl_fracl;
  unsigned int dbl_frach : 20;
  unsigned int dbl_exp : 11;
  unsigned int dbl_sign : 1;
};
int fpclassify(double d) {
  struct ieee_double* p = (struct ieee_double*)&d;

  if (p->dbl_exp == 0) {
    if (p->dbl_frach == 0 && p->dbl_fracl == 0)
      return FP_ZERO;
    else
      return FP_SUBNORMAL;
  }

  if (p->dbl_exp == DBL_EXP_INFNAN) {
    if (p->dbl_frach == 0 && p->dbl_fracl == 0)
      return FP_INFINITE;
    else
      return FP_NAN;
  }

  return FP_NORMAL;
}
