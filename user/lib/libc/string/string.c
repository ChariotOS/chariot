#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) (((x)-ONES) & (~(x)&HIGHS))

static inline int isdigit(int ch) { return (unsigned int)ch - '0' < 10; }

static inline int isspace(int ch) {
  return ch == ' ' || (unsigned int)ch - '\t' < 5;
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
