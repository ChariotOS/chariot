#include <arch.h>
#include <asm.h>
#include <atom.h>
#include <cpu.h>
#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <dev/driver.h>
#include <dev/mbr.h>
#include <dev/serial.h>
#include <pctl.h>
#include <fs/vfs.h>
#include <func.h>
#include <kargs.h>
#include <module.h>
#include <pci.h>
#include <pctl.h>
#include <phys.h>
#include <pit.h>
#include <printk.h>
#include <syscall.h>
#include <ptr.h>
#include <sched.h>
#include <set.h>
#include <smp.h>
#include <string.h>
#include <types.h>
#include <util.h>
#include <vec.h>
#include <vga.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void *, void *);

struct multiboot_info *mbinfo;
static void kmain2(void);

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/**
 * the size of the (main cpu) scheduler stack
 */
#define STKSIZE (4096 * 2)

extern void rtc_init(void);

extern "C" [[noreturn]] void kmain(u64 mbd, u64 magic) {
  rtc_init();
  serial_install();

  vga::early_init();

  extern u8 boot_cpu_local[];
  cpu::seginit(boot_cpu_local);

  arch::mem_init(mbd);

  mbinfo = (struct multiboot_info *)(u64)p2v(mbd);
  void *new_stack = (void *)((u64)kmalloc(STKSIZE) + STKSIZE);
  call_with_new_stack(new_stack, (void *)kmain2);
  while (1) panic("should not have gotten back here\n");
}

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];

static void call_global_constructors(void) {
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }
}

// kernel/init.cpp
int kernel_init(void *);

static void kmain2(void) {
  irq::init();
  enable_sse();

  call_global_constructors();

  vga::late_init();
  kargs::init(mbinfo);

  if (!smp::init()) panic("smp failed!\n");
  KINFO("Discovered SMP Cores\n");
  smp::lapic_init();

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // create the initialization thread.
  sched::proc::create_kthread(kernel_init);

  KINFO("starting scheduler\n");
  arch::sti();
  sched::run();

  panic("sched::run() returned\n");
  // [noreturn]
}
