#pragma once

/* Support non-nearest rounding mode.  */
#define WANT_ROUNDING 1
/* Support signaling NaNs.  */
#define WANT_SNAN 0


#if WANT_SNAN
#error SNaN is unsupported
#else
#define issignalingf_inline(x) 0
#define issignaling_inline(x) 0
#endif


#define asuint(f) \
  ((union {       \
    float _f;     \
    uint32_t _i;  \
  }){f})          \
      ._i
#define asfloat(i) \
  ((union {        \
    uint32_t _i;   \
    float _f;      \
  }){i})           \
      ._f
#define asuint64(f) \
  ((union {         \
    double _f;      \
    uint64_t _i;    \
  }){f})            \
      ._i
#define asdouble(i) \
  ((union {         \
    uint64_t _i;    \
    double _f;      \
  }){i})            \
      ._f


#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)


static inline double __math_invalid(double x) {
  return (x - x) / (x - x);
}

static inline double fp_barrier(double x) {
  volatile double y = x;
  return y;
}

static inline float fp_barrierf(float x) {
  volatile float y = x;
  return y;
}

static inline float eval_as_float(float x) {
  float y = x;
  return y;
}

static inline double eval_as_double(double x) {
  double y = x;
  return y;
}


#ifndef fp_force_evalf
#define fp_force_evalf fp_force_evalf
static inline void fp_force_evalf(float x) {
  volatile float y;
  y = x;
}
#endif

#ifndef fp_force_eval
#define fp_force_eval fp_force_eval
static inline void fp_force_eval(double x) {
  volatile double y;
  y = x;
}
#endif

