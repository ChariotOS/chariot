#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <printk.h>
#include <serial.h>
#include <types.h>
#include <paging.h>
#include <mem.h>
#include <idt.h>



u64 strlen(const char *str) {
  const char *s;
  for (s = str; *s; ++s)
    ;
  return (s - str);
}

extern int kernel_end;

int kmain(void) {

  serial_install();


  init_idt();

  // at this point, we are still mapped with the 2mb large pages.
  // init_mem will replace this with a more fine-grained 4k page system by mapping
  // kernel memory 1:1
  init_mem();

  while (1) halt();
  return 0;
}


