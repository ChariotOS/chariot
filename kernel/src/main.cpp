#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <posix.h>
#include <printk.h>
#include <serial.h>
#include <types.h>
#include <virtio_mmio.h>
#include "../../include/mobo/multiboot.h"

#include <device.h>
#include <phys.h>

#include <vec.h>

extern int kernel_end;

u64 fib(u64 n) {
  if (n < 2) return 1;

  return fib(n - 1) + fib(n - 2);
}

/*
static u64 strlen(char *str) {
  u64 i = 0;
  for (i = 0; str[i] != '\0'; i++)
    ;
  return i;
}
*/

// in src/arch/x86/sse.asm
extern "C" void enable_sse();

void initialize_kernel_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info* dev = __start__kernel_modules;
  int i = 0;
  while (dev != __stop__kernel_modules) {
    dev->initfn();
    dev = &(__start__kernel_modules[++i]);
  }
}

/*
typedef void (*func_ptr)(void);
extern func_ptr _init_array_start[0], _init_array_end[0];
extern func_ptr _fini_array_start[0], _fini_array_end[0];
*/

static void call_global_constructors(void) {
  /*
  for (func_ptr* func = _init_array_start; func != _init_array_end; func++)
    (*func)();
    */
}
/*
func_ptr _init_array_start[0] __attribute__((used, section(".init_array"),
                                             aligned(sizeof(func_ptr)))) = {};
func_ptr _fini_array_start[0] __attribute__((used, section(".fini_array"),
                                             aligned(sizeof(func_ptr)))) = {};

                                             */

extern "C" int kmain(u64 mbd, u64 magic) {
  // initialize the serial "driver"
  serial_install();

  init_idt();
  // now that we have interupts working, enable sse! (fpu)
  enable_sse();
  // at this point, we are still mapped with the 2mb large pages.
  // init_mem will replace this with a more fine-grained 4k page system by
  // mapping kernel memory 1:1
  init_mem(mbd);


  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  int i = 0;

  char buf[50];
  while (true) {
    void *p = phys::alloc();
    printk("%5d %p (%s)\n", i++, p, human_size(phys::nfree() * PGSIZE, buf));
    phys::free(p);
    continue;
  }


  // now that we have a decent programming enviroment, we can go through and
  // register all the kernel modules
  // NOTE: kernel module init code should not do any work outside of registering
  //       andlers, as the enviroment they are run in is fairly minimal and have
  //       no complex things like processes or devices.
  initialize_kernel_modules();

  // Enumerate PCI devices and assign drivers
  init_pci();

  assign_drivers();

  // initialize the programmable interrupt timer
  init_pit();
  // and set the interval, or whatever
  set_pit_freq(100);

  // finally, enable interrupts
  // NOT WORKING, WILL CAUSE DBLFLT INTR
  sti();

  // now try and exit.
  // write to the mobo exit port (special port)
  outb(0xE817, 0);

  // simply hltspin
  while (1) halt();
  return 0;
}

