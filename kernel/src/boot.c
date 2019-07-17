#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <types.h>
#include <asm.h>
#include <printk.h>

u64 strlen(const char *str) {
  const char *s;
  for (s = str; *s; ++s)
    ;
  return (s - str);
}


extern int kernel_end;

int kmain(void) {

  int x = 42;
  printk("woot: %#x\n", x);

  while(1) halt();
  return 0;
}
