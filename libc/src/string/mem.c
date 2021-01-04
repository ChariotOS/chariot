#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// memory related functions
// ie: memcpy, memmove, etc...

// most of the code here was coppied from musl :)




void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  for (size_t i = 0; i < n; i++) d[i] = s[i];
  return dest;
}



#ifdef __GNUC__
typedef __attribute__((__may_alias__)) size_t WT;
#define WS (sizeof(WT))
#endif

void *memmove(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;

  if (d == s) return d;
  if ((uintptr_t)s - (uintptr_t)d - n <= -2 * n) return memcpy(d, s, n);

  if (d < s) {
#ifdef __GNUC__
    if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
      while ((uintptr_t)d % WS) {
        if (!n--) return dest;
        *d++ = *s++;
      }
      for (; n >= WS; n -= WS, d += WS, s += WS) *(WT *)d = *(WT *)s;
    }
#endif
    for (; n; n--) *d++ = *s++;
  } else {
#ifdef __GNUC__
    if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
      while ((uintptr_t)(d + n) % WS) {
        if (!n--) return dest;
        d[n] = s[n];
      }
      while (n >= WS) n -= WS, *(WT *)(d + n) = *(WT *)(s + n);
    }
#endif
    while (n) n--, d[n] = s[n];
  }

  return dest;
}

// set every byte in a buffer of size n to c
void *memset(void *dest, int c, size_t n) {
  unsigned char *s = dest;
  size_t k;

  /* Fill head and tail with minimal branching. Each
   * conditional ensures that all the subsequently used
   * offsets are well-defined and in the dest region. */

  if (!n) return dest;
  s[0] = c;
  s[n - 1] = c;
  if (n <= 2) return dest;
  s[1] = c;
  s[2] = c;
  s[n - 2] = c;
  s[n - 3] = c;
  if (n <= 6) return dest;
  s[3] = c;
  s[n - 4] = c;
  if (n <= 8) return dest;

  /* Advance pointer to align it at a 4-byte boundary,
   * and truncate n to a multiple of 4. The previous code
   * already took care of any head/tail that get cut off
   * by the alignment. */

  k = -(uintptr_t)s & 3;
  s += k;
  n -= k;
  n &= -4;

  typedef uint32_t __attribute__((__may_alias__)) u32;
  typedef uint64_t __attribute__((__may_alias__)) u64;

  u32 c32 = ((u32)-1) / 255 * (unsigned char)c;

  /* In preparation to copy 32 bytes at a time, aligned on
   * an 8-byte bounary, fill head/tail up to 28 bytes each.
   * As in the initial byte-based head/tail fill, each
   * conditional below ensures that the subsequent offsets
   * are valid (e.g. !(n<=24) implies n>=28). */

  *(u32 *)(s + 0) = c32;
  *(u32 *)(s + n - 4) = c32;
  if (n <= 8) return dest;
  *(u32 *)(s + 4) = c32;
  *(u32 *)(s + 8) = c32;
  *(u32 *)(s + n - 12) = c32;
  *(u32 *)(s + n - 8) = c32;
  if (n <= 24) return dest;
  *(u32 *)(s + 12) = c32;
  *(u32 *)(s + 16) = c32;
  *(u32 *)(s + 20) = c32;
  *(u32 *)(s + 24) = c32;
  *(u32 *)(s + n - 28) = c32;
  *(u32 *)(s + n - 24) = c32;
  *(u32 *)(s + n - 20) = c32;
  *(u32 *)(s + n - 16) = c32;

  /* Align to a multiple of 8 so we can fill 64 bits at a time,
   * and avoid writing the same bytes twice as much as is
   * practical without introducing additional branching. */

  k = 24 + ((uintptr_t)s & 4);
  s += k;
  n -= k;

  /* If this loop is reached, 28 tail bytes have already been
   * filled, so any remainder when n drops below 32 can be
   * safely ignored. */

  u64 c64 = c32 | ((u64)c32 << 32);
  for (; n >= 32; n -= 32, s += 32) {
    *(u64 *)(s + 0) = c64;
    *(u64 *)(s + 8) = c64;
    *(u64 *)(s + 16) = c64;
    *(u64 *)(s + 24) = c64;
  }

  return dest;
}

// compare two memory regions
int memcmp(const void *vl, const void *vr, size_t n) {
  const unsigned char *l = vl, *r = vr;
  for (; n && *l == *r; n--, l++, r++)
    ;
  return n ? *l - *r : 0;
}

void *memrchr(const void *m, int c, size_t n) {
  const unsigned char *s = m;
  c = (unsigned char)c;
  while (n--)
    if (s[n] == c) return (void *)(s + n);
  return 0;
}
