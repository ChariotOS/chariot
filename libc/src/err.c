#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

__dead void err(int eval, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  verr(eval, fmt, ap);
  va_end(ap);
}
// DEF_WEAK(err);


__dead void errc(int eval, int code, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  verrc(eval, code, fmt, ap);
  va_end(ap);
}
// DEF_WEAK(errc);


__dead void errx(int eval, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  verrx(eval, fmt, ap);
  va_end(ap);
}
// DEF_WEAK(errx);



__dead void verr(int eval, const char *fmt, va_list ap) {
  int sverrno;

  sverrno = errno;
  (void)fprintf(stderr, "%s: ", __progname);
  if (fmt != NULL) {
    (void)vfprintf(stderr, fmt, ap);
    (void)fprintf(stderr, ": ");
  }
  (void)fprintf(stderr, "%s\n", strerror(sverrno));
  exit(eval);
}
// DEF_WEAK(verr);


__dead void verrc(int eval, int code, const char *fmt, va_list ap) {
  (void)fprintf(stderr, "%s: ", __progname);
  if (fmt != NULL) {
    (void)vfprintf(stderr, fmt, ap);
    (void)fprintf(stderr, ": ");
  }
  (void)fprintf(stderr, "%s\n", strerror(code));
  exit(eval);
}
// DEF_WEAK(verrc);



__dead void verrx(int eval, const char *fmt, va_list ap) {
  (void)fprintf(stderr, "%s: ", __progname);
  if (fmt != NULL) (void)vfprintf(stderr, fmt, ap);
  (void)fprintf(stderr, "\n");
  exit(eval);
}



void vwarn(const char *fmt, va_list ap) {
  int sverrno;

  sverrno = errno;
  (void)fprintf(stderr, "%s: ", __progname);
  if (fmt != NULL) {
    (void)vfprintf(stderr, fmt, ap);
    (void)fprintf(stderr, ": ");
  }
  (void)fprintf(stderr, "%s\n", strerror(sverrno));
}
// DEF_WEAK(vwarn);



void vwarnc(int code, const char *fmt, va_list ap) {
  (void)fprintf(stderr, "%s: ", __progname);
  if (fmt != NULL) {
    (void)vfprintf(stderr, fmt, ap);
    (void)fprintf(stderr, ": ");
  }
  (void)fprintf(stderr, "%s\n", strerror(code));
}
// DEF_WEAK(vwarnc);


void vwarnx(const char *fmt, va_list ap) {
  (void)fprintf(stderr, "%s: ", __progname);
  if (fmt != NULL) (void)vfprintf(stderr, fmt, ap);
  (void)fprintf(stderr, "\n");
}
// DEF_WEAK(vwarnx);


void warn(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vwarn(fmt, ap);
  va_end(ap);
}
// DEF_WEAK(warn);




void warnc(int code, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vwarnc(code, fmt, ap);
  va_end(ap);
}
// DEF_WEAK(warnc);

void warnx(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vwarnx(fmt, ap);
  va_end(ap);
}
// DEF_WEAK(warnx);
