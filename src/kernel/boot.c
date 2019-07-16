#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

typedef unsigned long ulong_t;
typedef void *addr_t;

#define hlt() __asm__ volatile("hlt")


static int fib(int);


void kmain(void) {
  fib(20);
  while (1) hlt();
}


static int fib(int n) {
  if (n < 2) return 1;
  return fib(n-1) + fib(n-2);
}
