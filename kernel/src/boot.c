#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))


#include <types.h>
#include <asm.h>

u64 strlen(const char *str) {
  const char *s;

  for (s = str; *s; ++s)
    ;
  return (s - str);
}


extern int kernel_end;

/* reverse:  reverse string s in place */
void reverse(char s[]) {
  int i, j;
  char c;

  for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

/* itoa:  convert n to characters in s */
void itoa(long long n, char s[]) {
  int i, sign;

  if ((sign = n) < 0) /* record sign */
    n = -n;           /* make n positive */
  i = 0;
  do {                     /* generate digits in reverse order */
    s[i++] = n % 10 + '0'; /* get next digit */
  } while ((n /= 10) > 0); /* delete it */
  if (sign < 0) s[i++] = '-';
  s[i] = '\0';
  reverse(s);
}

#define IO_PORT_PUTCHAR 0xfad
void puts(char *s) {
  int len = strlen(s);
  for (int i = 0; i < len; i++) outb(s[i], IO_PORT_PUTCHAR);
}

void print_long(u64 i) {
  char buf[64];  // a 64 bit number should never be this long.
  itoa(i, buf);
  puts(buf);
}

void print_ptr(void *ptr) {
  u64 p = (u64)ptr;
  static const char * charset = "0123456789abcdef";
  puts("0x"); // little dodad at the start
  for (int i = 15; i >= 0; i--) {
    char b = (p >> ((i) * 4)) & 0xf;
    outb(charset[(int)b], IO_PORT_PUTCHAR);
  }
}

#define hlt() __asm__ volatile("hlt")
int fib(int n) {
    unsigned int prev = 0;
    unsigned int result = 1;
    unsigned int summed;
    int i;
    if (n <= 0) {
      return 1;
    }
    if (n < 2) return n;
    for (i = 2; i <= n; i++) {
        summed = result + prev;
        prev = result;
        result = summed;
    }
    return result;
}

int x = 0;
int kmain(void) {

  print_ptr(&kernel_end);
  /*
  while (1) {
    outb(0, 0xff);
    fib(20);
    outb(0, 0xee);
  }
  */

  while(1) hlt();
  /*
  for (char *i = 0; 1; i += 4096) {
    char c = * i;
    print_ptr(i);
    puts("\n");
  }
  */

  while (1) hlt();
  return 0;
}
