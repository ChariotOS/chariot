#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>

int socket(int dom, int typ, int prot) {
  return errno_syscall(SYS_socket, dom, typ, prot);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
	       const struct sockaddr *dest_addr, size_t addrlen) {
  return errno_syscall(SYS_sendto, sockfd, buf, len, flags, dest_addr, addrlen);
}

int bind(int sockfd, struct sockaddr *addr, size_t len) {
  return errno_syscall(SYS_bind, sockfd, addr, len);
}

static uint16_t bswap_16(uint16_t __x) { return __x << 8 | __x >> 8; }

static uint32_t bswap_32(uint32_t __x) {
  return __x >> 24 | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000) | __x << 24;
}

uint16_t htons(uint16_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_16(n) : n;
}

uint32_t htonl(uint32_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_32(n) : n;
}

uint16_t ntohs(uint16_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_16(n) : n;
}

uint32_t ntohl(uint32_t n) {
  union {
    int i;
    char c;
  } u = {1};
  return u.c ? bswap_32(n) : n;
}

in_addr_t inet_addr(const char *p) {
  struct in_addr a;
  if (!inet_aton(p, &a)) return -1;
  return a.s_addr;
}

int inet_aton(const char *s0, struct in_addr *dest) {
  unsigned char *d = (void *)dest;

  uint32_t out[4];
  int n = sscanf(s0, "%d.%d.%d.%d", &out[0], &out[1], &out[2], &out[3]);
  if (n != 4) return 0;

  for (int i = 0; i < 4; i++) {
    if (out[i] > 255) return 0;
    d[i] = out[i];
  }
  return 1;
}

in_addr_t inet_network(const char *p) { return ntohl(inet_addr(p)); }

char *inet_ntoa(struct in_addr in) {
  static char buf[16];
  unsigned char *a = (void *)&in;
  snprintf(buf, sizeof buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
  return buf;
}

static int hexval(unsigned c) {
  if (c - '0' < 10) return c - '0';
  c |= 32;
  if (c - 'a' < 6) return c - 'a' + 10;
  return -1;
}

static inline int isdigit(int c) { return (unsigned)c - '0' < 10; }

int inet_pton(int af, const char *restrict s, void *restrict a0) {
  uint16_t ip[8];
  unsigned char *a = a0;
  int i, j, v, d, brk = -1, need_v4 = 0;

  if (af == AF_INET) {
    for (i = 0; i < 4; i++) {
      for (v = j = 0; j < 3 && isdigit(s[j]); j++) v = 10 * v + s[j] - '0';
      if (j == 0 || (j > 1 && s[0] == '0') || v > 255) return 0;
      a[i] = v;
      if (s[j] == 0 && i == 3) return 1;
      if (s[j] != '.') return 0;
      s += j + 1;
    }
    return 0;
  } else if (af != AF_INET6) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  if (*s == ':' && *++s != ':') return 0;

  for (i = 0;; i++) {
    if (s[0] == ':' && brk < 0) {
      brk = i;
      ip[i & 7] = 0;
      if (!*++s) break;
      if (i == 7) return 0;
      continue;
    }
    for (v = j = 0; j < 4 && (d = hexval(s[j])) >= 0; j++) v = 16 * v + d;
    if (j == 0) return 0;
    ip[i & 7] = v;
    if (!s[j] && (brk >= 0 || i == 7)) break;
    if (i == 7) return 0;
    if (s[j] != ':') {
      if (s[j] != '.' || (i < 6 && brk < 0)) return 0;
      need_v4 = 1;
      i++;
      break;
    }
    s += j + 1;
  }
  if (brk >= 0) {
    memmove(ip + brk + 7 - i, ip + brk, 2 * (i + 1 - brk));
    for (j = 0; j < 7 - i; j++) ip[brk + j] = 0;
  }
  for (j = 0; j < 8; j++) {
    *a++ = ip[j] >> 8;
    *a++ = ip[j];
  }
  if (need_v4 && inet_pton(AF_INET, (void *)s, a - 4) <= 0) return 0;
  return 1;
}

const char *inet_ntop(int af, const void *restrict a0, char *restrict s,
		      int l) {
  const unsigned char *a = a0;
  int i, j, max, best;
  char buf[100];

  switch (af) {
    case AF_INET:
      if (snprintf(s, l, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]) < l) return s;
      break;
    case AF_INET6:
      if (memcmp(a, "\0\0\0\0\0\0\0\0\0\0\377\377", 12))
	snprintf(buf, sizeof buf, "%x:%x:%x:%x:%x:%x:%x:%x", 256 * a[0] + a[1],
		 256 * a[2] + a[3], 256 * a[4] + a[5], 256 * a[6] + a[7],
		 256 * a[8] + a[9], 256 * a[10] + a[11], 256 * a[12] + a[13],
		 256 * a[14] + a[15]);
      else
	snprintf(buf, sizeof buf, "%x:%x:%x:%x:%x:%x:%d.%d.%d.%d",
		 256 * a[0] + a[1], 256 * a[2] + a[3], 256 * a[4] + a[5],
		 256 * a[6] + a[7], 256 * a[8] + a[9], 256 * a[10] + a[11],
		 a[12], a[13], a[14], a[15]);
      /* Replace longest /(^0|:)[:0]{2,}/ with "::" */
      for (i = best = 0, max = 2; buf[i]; i++) {
	if (i && buf[i] != ':') continue;
	j = strspn(buf + i, ":0");
	if (j > max) best = i, max = j;
      }
      if (max > 3) {
	buf[best] = buf[best + 1] = ':';
	memmove(buf + best + 2, buf + best + max, i - best - max + 1);
      }
      if (strlen(buf) < l) {
	strcpy(s, buf);
	return s;
      }
      break;
    default:
      errno = EAFNOSUPPORT;
      return 0;
  }
  errno = ENOSPC;
  return 0;
}

struct in_addr inet_makeaddr(in_addr_t n, in_addr_t h) {
  if (n < 256)
    h |= n << 24;
  else if (n < 65536)
    h |= n << 16;
  else
    h |= n << 8;
  return (struct in_addr){h};
}

in_addr_t inet_lnaof(struct in_addr in) {
  uint32_t h = in.s_addr;
  if (h >> 24 < 128) return h & 0xffffff;
  if (h >> 24 < 192) return h & 0xffff;
  return h & 0xff;
}

in_addr_t inet_netof(struct in_addr in) {
  uint32_t h = in.s_addr;
  if (h >> 24 < 128) return h >> 24;
  if (h >> 24 < 192) return h >> 16;
  return h >> 8;
}
