#include <wctype.h>
#include <stddef.h>

static const unsigned char table[] = {
#include "alpha.h"
};


size_t wcslen(const wchar_t *s) {
  const wchar_t *a;
  for (a = s; *s; s++)
    ;
  return s - a;
}
int iswdigit(wint_t wc) { return ((unsigned)wc - '0') < 10; }

int iswalpha(wint_t wc) {
  if (wc < 0x20000U) return (table[table[wc >> 8] * 32 + ((wc & 255) >> 3)] >> (wc & 7)) & 1;
  if (wc < 0x2fffeU) return 1;
  return 0;
}

int iswalnum(wint_t wc) { return iswdigit(wc) || iswalpha(wc); }

wchar_t *wcschr(const wchar_t *s, wchar_t c) {
  if (!c) return (wchar_t *)s + wcslen(s);
  for (; *s && *s != c; s++)
    ;
  return *s ? (wchar_t *)s : 0;
}


/* Our definition of whitespace is the Unicode White_Space property,
 * minus non-breaking spaces (U+00A0, U+2007, and U+202F) and script-
 * specific characters with non-blank glyphs (U+1680 and U+180E). */

int iswspace(wint_t wc) {
  static const wchar_t spaces[] = {' ', '\t', '\n', '\r', 11, 12, 0x0085, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2008,
      0x2009, 0x200a, 0x2028, 0x2029, 0x205f, 0x3000, 0};
  return wc && wcschr(spaces, wc);
}
