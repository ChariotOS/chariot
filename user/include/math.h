
#ifndef _MATH_H
#define _MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HUGE_VAL 1e10000
#define M_E 2.718281828459045

#define M_PI 3.14159265358979323846   /* pi */
#define M_PI_2 1.57079632679489661923 /* pi/2 */
#define M_PI_4 0.78539816339744830962 /* pi/4 */

#define M_1_PI 0.31830988618379067154     /* 1/pi */
#define M_2_PI 0.63661977236758134308     /* 2/pi */
#define M_2_SQRTPI 1.12837916709551257390 /* 2/sqrt(pi) */

#define M_TAU (M_PI * 2)
#define M_LN2 0.69314718055995
#define M_LN10 2.30258509299405

#define M_LOG2E 1.4426950408889634074    /* log_2 e */
#define M_LOG10E 0.43429448190325182765  /* log_10 e */
#define M_SQRT2 1.41421356237309504880   /* sqrt(2) */
#define M_SQRT1_2 0.70710678118654752440 /* 1/sqrt(2) */

double acos(double);
float acosf(float);
double asin(double);
float asinf(float);
double atan(double);
float atanf(float);
double atan2(double, double);
float atan2f(float, float);
double cos(double);
float cosf(float);
double cosh(double);
float coshf(float);
double sin(double);
float sinf(float);
double sinh(double);
float sinhf(float);
double tan(double);
float tanf(float);
double tanh(double);
float tanhf(float);
double ceil(double);
float ceilf(float);
double floor(double);
float floorf(float);
double round(double);
float roundf(float);
double fabs(double);
float fabsf(float);
double fmod(double, double);
float fmodf(float, float);
double exp(double);
float expf(float);
double frexp(double, int* exp);
float frexpf(float, int* exp);
double log(double);
float logf(float);
double log10(double);
float log10f(float);
double sqrt(double);
float sqrtf(float);
double modf(double, double*);
float modff(float, float*);
double ldexp(double, int exp);
float ldexpf(float, int exp);

double pow(double x, double y);

double log2(double);
float log2f(float);
long double log2l(long double);
double frexp(double, int*);
float frexpf(float, int*);
long double frexpl(long double, int*);

#ifdef __cplusplus
}
#endif

#endif
