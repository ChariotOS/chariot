#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <idt.h>
#include <mem.h>
#include <paging.h>
#include <printk.h>
#include <serial.h>
#include <types.h>

#define MOBO_WELCOME                     \
  "___  ______________  _____    \n"     \
  "|  \\/  |  _  | ___ \\|  _  |   \n"   \
  "| .  . | | | | |_/ /| | | |   \n"     \
  "| |\\/| | | | | ___ \\| | | |   \n"   \
  "| |  | \\ \\_/ / |_/ /\\ \\_/ /   \n" \
  "\\_|  |_/\\___/\\____/  \\___/    \n" \
  "                              \n"

u64 strlen(const char *str) {
  const char *s;
  for (s = str; *s; ++s)
    ;
  return (s - str);
}

extern int kernel_end;
// in src/arch/x86/sse.asm
extern void enable_sse();

int kmain(void) {

  serial_install();


  // at this point, we are still mapped with the 2mb large pages.
  // init_mem will replace this with a more fine-grained 4k page system by
  // mapping kernel memory 1:1
  init_mem();

  init_idt();


  // now that we have interupts working, enable sse! (fpu)
  enable_sse();


  /*
  char *c = NULL;
  printk("%02x\n", *c);
  */


  // simply hltspin
  while (1) halt();
  return 0;
}

