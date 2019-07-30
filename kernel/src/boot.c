#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <idt.h>
#include <mem.h>
#include <paging.h>
#include <printk.h>
#include <serial.h>
#include <types.h>

extern int kernel_end;

u64 fib(u64 n) {
  if (n < 2) return 1;

  return fib(n - 1) + fib(n - 2);
}

#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02
#define O_CREAT 0100
#define O_APPEND 02000

// do_posix_syscall expects 6 ints and returns one int
extern int do_posix_syscall(u64 args[6]);

int open(char *filename, int flags, int opts) {
  u64 args[6];
  args[0] = 0x05; // sys_open
  args[1] = (u64)filename;
  args[2] = flags;
  args[3] = opts;
  return do_posix_syscall(args);
}

// in src/arch/x86/sse.asm
extern void enable_sse();

int kmain(void) {
  serial_install();
  init_idt();

  // at this point, we are still mapped with the 2mb large pages.
  // init_mem will replace this with a more fine-grained 4k page system by
  // mapping kernel memory 1:1
  init_mem();

  // now that we have interupts working, enable sse! (fpu)
  enable_sse();

  // finally, enable interrupts
  sti();

  int res = open("foo bar baz", O_CREAT | O_APPEND, 0666);
  printk("res: %d\n", res);

  // fib(30);
  // for (int i = 0; true; i++) printk("%016x ", i);

  // go do the thing
  printk("hello from inside the kernel\n");

  // simply hltspin
  while (1) halt();
  return 0;
}

