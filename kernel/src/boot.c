#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <printk.h>
#include <serial.h>
#include <types.h>
#include <paging.h>
#include <mem.h>




u64 strlen(const char *str) {
  const char *s;
  for (s = str; *s; ++s)
    ;
  return (s - str);
}

extern int kernel_end;

int kmain(void) {
  serial_install();
  init_mem();

  /*
  char *addr = 0;
  while (true) {
    char *pa = get_physaddr(addr);
    printk("%p -> %p\n", addr, pa);
    addr += 4096;
  }
  */


  for (int i = 0; i < 4096; i++) {
    void *addr = alloc_page();
    char *pa = get_physaddr(addr);
    printk("%-3d  %p -> %p\n", i, addr, pa);
  }



  while (1) halt();
  return 0;
}


