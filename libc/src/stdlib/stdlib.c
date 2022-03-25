#define _CHARIOT_SRC
#include <chariot.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/sysbind.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;


static void insertion_sort(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
  int cnt;
  unsigned char ch;
  char *s1, *s2, *t1, *t2, *top;
  top = (char *)bot + nmemb * size;
  for (t1 = (char *)bot + size; t1 < top;) {
    for (t2 = t1; (t2 -= size) >= (char *)bot && compar(t1, t2) < 0;)
      ;
    if (t1 != (t2 += size)) {
      for (cnt = size; cnt--; ++t1) {
        ch = *t1;
        for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
          *s1 = *s2;
        *s1 = ch;
      }
    } else
      t1 += size;
  }
}

static void insertion_sort_r(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg) {
  int cnt;
  unsigned char ch;
  char *s1, *s2, *t1, *t2, *top;
  top = (char *)bot + nmemb * size;
  for (t1 = (char *)bot + size; t1 < top;) {
    for (t2 = t1; (t2 -= size) >= (char *)bot && compar(t1, t2, arg) < 0;)
      ;
    if (t1 != (t2 += size)) {
      for (cnt = size; cnt--; ++t1) {
        ch = *t1;
        for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
          *s1 = *s2;
        *s1 = ch;
      }
    } else
      t1 += size;
  }
}

void qsort(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
  if (nmemb <= 1) return;

  insertion_sort(bot, nmemb, size, compar);
}

void qsort_r(void *bot, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg) {
  if (nmemb <= 1) return;

  insertion_sort_r(bot, nmemb, size, compar, arg);
}


int system(const char *command) {
  int pid = fork();
  if (pid == 0) {
    const char *args[] = {"/bin/sh", "-c", (char *)command, NULL};

    // debug_hexdump(args, sizeof(args));
    execve("/bin/sh", args, (const char **)environ);
    exit(-1);
  }

  int stat;
  waitpid(pid, &stat, 0);
  // TODO: detect an error
  return 0;
}

static void __env_rm_add(char *old, char *new) {
  static char **env_alloced;
  static size_t env_alloced_n;
  for (size_t i = 0; i < env_alloced_n; i++)
    if (env_alloced[i] == old) {
      env_alloced[i] = new;
      free(old);
      return;
    } else if (!env_alloced[i] && new) {
      env_alloced[i] = new;
      new = 0;
    }
  if (!new) return;
  char **t = realloc(env_alloced, sizeof *t * (env_alloced_n + 1));
  if (!t) return;
  (env_alloced = t)[env_alloced_n++] = new;
}

static int __putenv(char *s, size_t l, char *r) {
  size_t i = 0;
  if (environ) {
    for (char **e = environ; *e; e++, i++)
      if (!strncmp(s, *e, l + 1)) {
        char *tmp = *e;
        *e = s;
        __env_rm_add(tmp, r);
        return 0;
      }
  }
  static char **oldenv;
  char **newenv;
  if (environ == oldenv) {
    newenv = realloc(oldenv, sizeof *newenv * (i + 2));
    if (!newenv) goto oom;
  } else {
    newenv = malloc(sizeof *newenv * (i + 2));
    if (!newenv) goto oom;
    if (i) memcpy(newenv, environ, sizeof *newenv * i);
    free(oldenv);
  }
  newenv[i] = s;
  newenv[i + 1] = 0;
  environ = oldenv = newenv;
  if (r) __env_rm_add(0, r);
  return 0;
oom:
  free(r);
  return -1;
}

char *getenv(const char *name) {
  size_t l = strchrnul(name, '=') - name;
  if (l && !name[l] && environ)
    for (char **e = environ; *e; e++)
      if (!strncmp(name, *e, l) && l[*e] == '=') return *e + l + 1;
  return 0;
}

int putenv(char *s) {
  size_t l = strchrnul(s, '=') - s;
  if (!l || !s[l]) return unsetenv(s);
  return __putenv(s, l, 0);
}

int setenv(const char *var, const char *value, int overwrite) {
  char *s;
  size_t l1, l2;

  if (!var || !(l1 = strchrnul(var, '=') - var) || var[l1]) {
    // TODO: errno
    // errno = EINVAL;
    return -1;
  }
  if (!overwrite && getenv(var)) return 0;

  l2 = strlen(value);
  s = malloc(l1 + l2 + 2);
  if (!s) return -1;
  memcpy(s, var, l1);
  s[l1] = '=';
  memcpy(s + l1 + 1, value, l2 + 1);
  return __putenv(s, l1, s);
}

int unsetenv(const char *name) {
  size_t l = strchrnul(name, '=') - name;
  if (!l || name[l]) {
    // TODO: errno
    // errno = EINVAL;
    return -1;
  }
  if (environ) {
    char **e = environ, **eo = e;
    for (; *e; e++)
      if (!strncmp(name, *e, l) && l[*e] == '=')
        __env_rm_add(*e, 0);
      else if (eo != e)
        *eo++ = *e;
      else
        eo++;
    if (eo != e) *eo = 0;
  }
  return 0;
}


void abort(void) {
  fprintf(stderr, "abort() called!\n");
  exit(EXIT_FAILURE);
}


// nonstandard
char *path_join(char *a, char *b) {
  int l = strlen(a) + 1;
  int k = strlen(b);

  char *dst = malloc(l + k + 1);
  char *z = a + l - 1;

  memcpy(dst, a, z - a);
  dst[z - a] = '/';
  memcpy(dst + (z - a) + (z > a), b, k + 1);

  return dst;
}


static unsigned seed;

static unsigned temper(unsigned x) {
  x ^= x >> 11;
  x ^= x << 7 & 0x9D2C5680;
  x ^= x << 15 & 0xEFC60000;
  x ^= x >> 18;
  return x;
}

int rand_r(unsigned *seed) { return temper(*seed = *seed * 1103515245 + 12345) / 2; }


void srand(unsigned s) { seed = s - 1; }

int rand(void) { return rand_r(&seed); }




#define C_RED 91
#define C_GREEN 92
#define C_YELLOW 93
#define C_BLUE 94
#define C_MAGENTA 95
#define C_CYAN 96

#define C_RESET 0
#define C_GRAY 90

static void set_color(int code) {
  static int current_color = 0;
  if (code != current_color) {
    printf("\x1b[%dm", code);
    current_color = code;
  }
}

static void set_color_for(char c) {
  if (c >= 'A' && c <= 'z') {
    set_color(C_YELLOW);
  } else if (c >= '!' && c <= '~') {
    set_color(C_CYAN);
  } else if (c == '\n' || c == '\r') {
    set_color(C_GREEN);
  } else if (c == '\a' || c == '\b' || c == 0x1b || c == '\f' || c == '\n' || c == '\r') {
    set_color(C_RED);
  } else if ((unsigned char)c == 0xFF) {
    set_color(C_MAGENTA);
  } else {
    set_color(C_GRAY);
  }
}



static int validate_address(void *p) {
  char buf[1];
  // lol this is a hack
  if (mgetname(p, buf, 1) == -ENOENT) {
    return 0;
  }
  return 1;
}

void debug_hexdump_grouped(void *vbuf, size_t len, int grouping) {
  unsigned awidth = 4;

  if (len > 0xFFFFL) awidth = 8;

  unsigned char *buf = (unsigned char *)vbuf;
  int w = 16;

  // array of valid address checks
  char valid[16];

  int has_validated = 0;
  off_t last_validated_page = 0;
  int is_valid = 0;

  for (unsigned long long i = 0; i < len; i += w) {
    unsigned char *line = buf + i;


    for (int c = 0; c < w; c++) {
      off_t page = (off_t)(line + c) >> 12;

      if (!has_validated || page != last_validated_page) {
        is_valid = validate_address(line + c);
        has_validated = 1;
      }

      valid[c] = is_valid;
      last_validated_page = page;
    }

    set_color(C_RESET);
    printf("|");
    set_color(C_GRAY);

    printf("%.*llx", awidth, i);

    set_color(C_RESET);
    printf("|");
    for (int c = 0; c < w; c++) {
      if (c % grouping == 0) printf(" ");

      if (valid[c] == 0) {
        set_color(C_RED);
        printf("??");
        continue;
      }

      if (i + c >= len) {
        set_color(C_GRAY);
        printf("##");
      } else {
        set_color_for(line[c]);
        printf("%02X", line[c]);
      }
    }

    set_color(C_RESET);
    printf(" |");
    for (int c = 0; c < w; c++) {
      if (c != 0 && (c % 8 == 0)) {
        set_color(C_RESET);
        printf(" ");
      }


      if (valid[c] == 0) {
        set_color(C_RED);
        printf("?");
        continue;
      }

      if (i + c >= len) {
        printf(" ");
      } else {
        set_color_for(line[c]);
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    set_color(C_RESET);
    printf("|\n");
  }
}

void debug_hexdump(void *vbuf, size_t len) { return debug_hexdump_grouped(vbuf, len, 1); }




long long strtoll_l(const char *__restrict nptr, char **__restrict endptr, int base) {
  const char *s;
  unsigned long long acc;
  char c;
  unsigned long long cutoff;
  int neg, any, cutlim;
  /*
   * Skip white space and pick up leading +/- sign if any.
   * If base is 0, allow 0x for hex and 0 for octal, else
   * assume decimal; if base is already 16, allow 0x.
   */
  s = nptr;
  do {
    c = *s++;
  } while (isspace((char)c));
  if (c == '-') {
    neg = 1;
    c = *s++;
  } else {
    neg = 0;
    if (c == '+') c = *s++;
  }
  if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X') &&
      ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') || (s[1] >= 'a' && s[1] <= 'f'))) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0) base = c == '0' ? 8 : 10;
  acc = any = 0;
  if (base < 2 || base > 36) goto noconv;

  /*
   * Compute the cutoff value between legal numbers and illegal
   * numbers.  That is the largest legal value, divided by the
   * base.  An input number that is greater than this value, if
   * followed by a legal input character, is too big.  One that
   * is equal to this value may be valid or not; the limit
   * between valid and invalid numbers is then based on the last
   * digit.  For instance, if the range for quads is
   * [-9223372036854775808..9223372036854775807] and the input base
   * is 10, cutoff will be set to 922337203685477580 and cutlim to
   * either 7 (neg==0) or 8 (neg==1), meaning that if we have
   * accumulated a value > 922337203685477580, or equal but the
   * next digit is > 7 (or 8), the number is too big, and we will
   * return a range error.
   *
   * Set 'any' if any `digits' consumed; make it negative to indicate
   * overflow.
   */
  cutoff = neg ? (unsigned long long)-(LLONG_MIN + LLONG_MAX) + LLONG_MAX : LLONG_MAX;
  cutlim = cutoff % base;
  cutoff /= base;
  for (;; c = *s++) {
    if (c >= '0' && c <= '9')
      c -= '0';
    else if (c >= 'A' && c <= 'Z')
      c -= 'A' - 10;
    else if (c >= 'a' && c <= 'z')
      c -= 'a' - 10;
    else
      break;
    if (c >= base) break;
    if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
      any = -1;
    else {
      any = 1;
      acc *= base;
      acc += c;
    }
  }
  if (any < 0) {
    acc = neg ? LLONG_MIN : LLONG_MAX;
    errno = ERANGE;
  } else if (!any) {
  noconv:
    errno = EINVAL;
  } else if (neg)
    acc = -acc;
  if (endptr != NULL) *endptr = (char *)(any ? s - 1 : nptr);
  return (acc);
}

long int strtol(const char *nptr, char **endptr, int base) { return strtoll(nptr, endptr, base); }
long long strtoll(const char *__restrict nptr, char **__restrict endptr, int base) { return strtoll_l(nptr, endptr, base); }


unsigned long strtoul(const char *nptr, char **endptr, int base) { return strtol(nptr, endptr, base); }
unsigned long long strtoull(const char *nptr, char **endptr, int base) { return strtoll(nptr, endptr, base); }

static char ptsname_buf[32];
char *ptsname(int fd) {
  int id = ioctl(fd, PTMX_GETPTSID);
  if (id < 0) return NULL;

  snprintf(ptsname_buf, 32, "/dev/vtty%d", id);
  return ptsname_buf;
}



int posix_memalign(void **memptr, size_t alignment, size_t size) {
  // printf("posix_memalign sz: %d, align: %d\n", size, alignment);
  *memptr = malloc(size);
  return 0;
}


void *bsearch(const void *key, const void *base, size_t nel, size_t width, int (*cmp)(const void *, const void *)) {
  void *try;
  int sign;
  while (nel > 0) {
    try = (char *)base + width * (nel / 2);
    sign = cmp(key, try);
    if (sign < 0) {
      nel /= 2;
    } else if (sign > 0) {
      base = (char *)try + width;
      nel -= nel / 2 + 1;
    } else {
      return try;
    }
  }
  return NULL;
}
