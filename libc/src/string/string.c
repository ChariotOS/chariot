#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) (((x)-ONES) & (~(x)&HIGHS))


char *strrchr(const char *str, int ch) {
  char *last = NULL;
  char c;
  for (; (c = *str); ++str) {
    if (c == ch) last = (char *)(str);
  }
  return last;
}

long atol(const char *s) {
  int n = 0;
  int neg = 0;
  while (isspace(*s)) {
    s++;
  }
  switch (*s) {
    case '-':
      neg = 1;
      /* Fallthrough is intentional here */
    case '+':
      s++;
  }
  while (isdigit(*s)) {
    n = 10 * n - (*s++ - '0');
  }
  /* The sign order may look incorrect here but this is correct as n is
   * calculated as a negative number to avoid overflow on INT_MAX.
   */
  return neg ? n : -n;
}

int atoi(const char *s) {
  int n = 0;
  int neg = 0;
  while (isspace(*s)) {
    s++;
  }
  switch (*s) {
    case '-':
      neg = 1;
      /* Fallthrough is intentional here */
    case '+':
      s++;
  }
  while (isdigit(*s)) {
    n = 10 * n - (*s++ - '0');
  }
  /* The sign order may look incorrect here but this is correct as n is
   * calculated as a negative number to avoid overflow on INT_MAX.
   */
  return neg ? n : -n;
}

double atof(const char *c) {
	double res = 0.0;
	sscanf(c, "%f", &res);
	return res;
}

char *strdup(const char *s) {
  size_t l = strlen(s);
  return memcpy(malloc(l + 1), s, l + 1);
}

char *stpcpy(char *restrict d, const char *restrict s) {
  size_t *wd;
  const size_t *ws;

  if ((uintptr_t)s % ALIGN == (uintptr_t)d % ALIGN) {
    for (; (uintptr_t)s % ALIGN; s++, d++) {
      if (!(*d = *s)) {
        return d;
      }
    }
    wd = (void *)d;
    ws = (const void *)s;
    for (; !HASZERO(*ws); *wd++ = *ws++)
      ;
    d = (void *)wd;
    s = (const void *)ws;
  }

  for (; (*d = *s); s++, d++)
    ;

  return d;
}

char *strcpy(char *restrict d, const char *restrict s) {
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  word *wd;
  const word *ws;
  if ((uintptr_t)s % ALIGN == (uintptr_t)d % ALIGN) {
    for (; (uintptr_t)s % ALIGN; s++, d++)
      if (!(*d = *s)) return d;
    wd = (void *)d;
    ws = (const void *)s;
    for (; !HASZERO(*ws); *wd++ = *ws++)
      ;
    d = (void *)wd;
    s = (const void *)ws;
  }
#endif
  for (; (*d = *s); s++, d++)
    ;

  return d;
}

char *strncpy(char *restrict d, const char *restrict s, size_t n) {
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  word *wd;
  const word *ws;
  if (((uintptr_t)s & ALIGN) == ((uintptr_t)d & ALIGN)) {
    for (; ((uintptr_t)s & ALIGN) && n && (*d = *s); n--, s++, d++)
      ;
    if (!n || !*s) goto tail;
    wd = (void *)d;
    ws = (const void *)s;
    for (; n >= sizeof(size_t) && !HASZERO(*ws);
         n -= sizeof(size_t), ws++, wd++)
      *wd = *ws;
    d = (void *)wd;
    s = (const void *)ws;
  }
#endif
  for (; n && (*d = *s); n--, s++, d++)
    ;
tail:
  memset(d, 0, n);
  return d;
}

char *strcat(char *dest, const char *src) {
  strcpy(dest + strlen(dest), src);
  return dest;
}

size_t strlen(const char *s) {
  const char *a = s;
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  const word *w;
  for (; (uintptr_t)s % ALIGN; s++)
    if (!*s) return s - a;
  for (w = (const void *)s; !HASZERO(*w); w++)
    ;
  s = (const void *)w;
#endif
  for (; *s; s++)
    ;
  return s - a;
}

char *strchr(const char *s, int c) {
  c = (unsigned char)c;
  if (!c) return (char *)s + strlen(s);

  for (; *s && *(unsigned char *)s != c; s++) {
  }
  char *r = (char *)s;

  return *(unsigned char *)r == (unsigned char)c ? r : 0;
}

int strcmp(const char *l, const char *r) {
  for (; *l == *r && *l; l++, r++)
    ;
  return *(unsigned char *)l - *(unsigned char *)r;
}

int strncmp(const char *_l, const char *_r, size_t n) {
  const unsigned char *l = (void *)_l, *r = (void *)_r;
  if (!n--) return 0;
  for (; *l && *r && n && *l == *r; l++, r++, n--)
    ;
  return *l - *r;
}


int strcoll(const char *s1, const char *s2) { return strcmp(s1, s2); }

#define BITOP(A, B, OP)                              \
  ((A)[(size_t)(B) / (8 * sizeof *(A))] OP(size_t) 1 \
   << ((size_t)(B) % (8 * sizeof *(A))))

size_t strspn(const char *s, const char *c) {
  const char *a = s;
  size_t byteset[32 / sizeof(size_t)] = {0};

  if (!c[0]) {
    return 0;
  }
  if (!c[1]) {
    for (; *s == *c; s++)
      ;
    return s - a;
  }

  for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++)
    ;
  for (; *s && BITOP(byteset, *(unsigned char *)s, &); s++)
    ;

  return s - a;
}

char *strchrnul(const char *s, int c) {
  size_t *w;
  size_t k;

  c = (unsigned char)c;
  if (!c) {
    return (char *)s + strlen(s);
  }

  for (; (uintptr_t)s % ALIGN; s++) {
    if (!*s || *(unsigned char *)s == c) {
      return (char *)s;
    }
  }

  k = ONES * c;
  for (w = (void *)s; !HASZERO(*w) && !HASZERO(*w ^ k); w++)
    ;
  for (s = (void *)w; *s && *(unsigned char *)s != c; s++)
    ;
  return (char *)s;
}

size_t strcspn(const char *s, const char *c) {
  const char *a = s;
  if (c[0] && c[1]) {
    size_t byteset[32 / sizeof(size_t)] = {0};
    for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++)
      ;
    for (; *s && !BITOP(byteset, *(unsigned char *)s, &); s++)
      ;
    return s - a;
  }
  return strchrnul(s, *c) - a;
}

char *strpbrk(const char *s, const char *b) {
  s += strcspn(s, b);
  return *s ? (char *)s : 0;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
  char *token;
  if (str == NULL) {
    str = *saveptr;
  }
  str += strspn(str, delim);
  if (*str == '\0') {
    *saveptr = str;
    return NULL;
  }
  token = str;
  str = strpbrk(token, delim);
  if (str == NULL) {
    *saveptr = (char *)strchr(token, '\0');
  } else {
    *str = '\0';
    *saveptr = str + 1;
  }
  return token;
}

char *strtok(char *str, const char *delim) {
  static char *saveptr = NULL;
  if (str) {
    saveptr = NULL;
  }
  return strtok_r(str, delim, &saveptr);
}

const int _strtod_maxExponent_ =
    511; /* Largest possible base 10 exponent.  Any
          * exponent larger than this will already
          * produce underflow or overflow, so there's
          * no need to worry about additional digits.
          */

const double _strtod_powersOf10_[] =
    {      /* Table giving binary powers of 10.  Entry */
     10.,  /* is 10^2^i.  Used to convert decimal */
     100., /* exponents into floating-point numbers. */
     1.0e4, 1.0e8, 1.0e16, 1.0e32, 1.0e64, 1.0e128, 1.0e256};

double strtod(const char *s, /* A decimal ASCII floating-point number,
                              * optionally preceded by white space.
                              * Must have form "-I.FE-X", where I is the
                              * integer part of the mantissa, F is the
                              * fractional part of the mantissa, and X
                              * is the exponent.  Either of the signs
                              * may be "+", "-", or omitted.  Either I
                              * or F may be omitted, or both.  The decimal
                              * point isn't necessary unless F is present.
                              * The "E" may actually be an "e".  E and X
                              * may both be omitted (but not just one).
                              */
              char **endPtr  /* If non-NULL, store terminating character's
                              * address here. */
) {
  int sign, expSign = 0;
  double fraction, dblExp;
  const double *d;
  char *p;
  int c;
  int exp = 0;     /* Exponent read from "EX" field. */
  int fracExp = 0; /* Exponent that derives from the fractional
                    * part.  Under normal circumstatnces, it is
                    * the negative of the number of digits in F.
                    * However, if I is very long, the last digits
                    * of I get dropped (otherwise a long I with a
                    * large negative exponent could cause an
                    * unnecessary overflow on I alone).  In this
                    * case, fracExp is incremented one for each
                    * dropped digit. */
  int mantSize;    /* Number of digits in mantissa. */
  int decPt;       /* Number of mantissa digits BEFORE decimal
                    * point. */
  char *pExp;      /* Temporarily holds location of exponent
                    * in string. */

  /*
   * Strip off leading blanks and check for a sign.
   */

  char *string = (char *)s;

  p = string;
  while (isspace(*p)) {
    p += 1;
  }
  if (*p == '-') {
    sign = 1;
    p += 1;
  } else {
    if (*p == '+') {
      p += 1;
    }
    sign = 0;
  }

  /*
   * Count the number of digits in the mantissa (including the decimal
   * point), and also locate the decimal point.
   */

  decPt = -1;
  for (mantSize = 0;; mantSize += 1) {
    c = *p;
    if (!isdigit(c)) {
      if ((c != '.') || (decPt >= 0)) {
        break;
      }
      decPt = mantSize;
    }
    p += 1;
  }

  /*
   * Now suck up the digits in the mantissa.  Use two integers to
   * collect 9 digits each (this is faster than using floating-point).
   * If the mantissa has more than 18 digits, ignore the extras, since
   * they can't affect the value anyway.
   */

  pExp = p;
  p -= mantSize;
  if (decPt < 0) {
    decPt = mantSize;
  } else {
    mantSize -= 1; /* One of the digits was the point. */
  }
  if (mantSize > 18) {
    fracExp = decPt - 18;
    mantSize = 18;
  } else {
    fracExp = decPt - mantSize;
  }
  if (mantSize == 0) {
    fraction = 0.0;
    p = string;
    goto done;
  } else {
    int frac1, frac2;
    frac1 = 0;
    for (; mantSize > 9; mantSize -= 1) {
      c = *p;
      p += 1;
      if (c == '.') {
        c = *p;
        p += 1;
      }
      frac1 = 10 * frac1 + (c - '0');
    }
    frac2 = 0;
    for (; mantSize > 0; mantSize -= 1) {
      c = *p;
      p += 1;
      if (c == '.') {
        c = *p;
        p += 1;
      }
      frac2 = 10 * frac2 + (c - '0');
    }
    fraction = (1.0e9 * frac1) + frac2;
  }

  /*
   * Skim off the exponent.
   */

  p = pExp;
  if ((*p == 'E') || (*p == 'e')) {
    p += 1;
    if (*p == '-') {
      expSign = 1;
      p += 1;
    } else {
      if (*p == '+') {
        p += 1;
      }
      expSign = 0;
    }
    while (isdigit(*p)) {
      exp = exp * 10 + (*p - '0');
      p += 1;
    }
  }
  if (expSign) {
    exp = fracExp - exp;
  } else {
    exp = fracExp + exp;
  }

  /*
   * Generate a floating-point number that represents the exponent.
   * Do this by processing the exponent one bit at a time to combine
   * many powers of 2 of 10. Then combine the exponent with the
   * fraction.
   */

  if (exp < 0) {
    expSign = 1;
    exp = -exp;
  } else {
    expSign = 0;
  }
  if (exp > _strtod_maxExponent_) {
    exp = _strtod_maxExponent_;
  }
  dblExp = 1.0;

  for (d = _strtod_powersOf10_; exp != 0; exp >>= 1, d += 1) {
    if (exp & 01) {
      dblExp *= *d;
    }
  }
  if (expSign) {
    fraction /= dblExp;
  } else {
    fraction *= dblExp;
  }

done:
  if (endPtr != NULL) {
    *endPtr = (char *)p;
  }

  if (sign) {
    return -fraction;
  }
  return fraction;
}

float strtof(const char *s, char **p) { return strtod(s, p); }

long double strtold(const char *restrict s, char **restrict p) {
  return strtod(s, p);
}

static const struct lconv posix_lconv = {
    .decimal_point = ".",
    .thousands_sep = "",
    .grouping = "",
    .int_curr_symbol = "",
    .currency_symbol = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping = "",
    .positive_sign = "",
    .negative_sign = "",
    .int_frac_digits = CHAR_MAX,
    .frac_digits = CHAR_MAX,
    .p_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .n_cs_precedes = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .int_p_cs_precedes = CHAR_MAX,
    .int_p_sep_by_space = CHAR_MAX,
    .int_n_cs_precedes = CHAR_MAX,
    .int_n_sep_by_space = CHAR_MAX,
    .int_p_sign_posn = CHAR_MAX,
    .int_n_sign_posn = CHAR_MAX,
};

struct lconv *localeconv(void) {
  return (void *)&posix_lconv;
}

void *memchr(const void *src, int c, size_t n) {
  const unsigned char *s = src;
  c = (unsigned char)c;
  for (; n && *s != c; s++, n--)
    ;
  return n ? (void *)s : 0;
}

char *strerror(int e) {
  switch (e) {
#define E(a, b) \
  case a:       \
    return #b;  \
    break;
#include "../stdio/__strerror.h"

    default:
      return "No error information";
  }
}




/* strings.h nonsense */
static char foldcase(char ch) {
  if (isalpha(ch)) return tolower(ch);
  return ch;
}

int strcasecmp(const char *s1, const char *s2) {
  for (; foldcase(*s1) == foldcase(*s2); ++s1, ++s2) {
    if (*s1 == 0) return 0;
  }
  return foldcase(*(const unsigned char *)s1) <
                 foldcase(*(const unsigned char *)s2)
             ? -1
             : 1;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
  if (!n) return 0;
  do {
    if (foldcase(*s1) != foldcase(*s2++))
      return foldcase(*(const unsigned char *)s1) -
             foldcase(*(const unsigned char *)--s2);
    if (*s1++ == 0) break;
  } while (--n);
  return 0;
}
