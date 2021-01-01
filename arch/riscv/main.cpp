#include <asm.h>
#include <cpu.h>
#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/plic.h>
#include <riscv/uart.h>
#include <util.h>
#include <devicetree.h>

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" char _kernel_end[];
extern "C" char _bss_start[];
extern "C" char _bss_end[];

volatile long count = 0;

/* lowlevel.S, calls kerneltrap() */
extern "C" void kernelvec(void);

void fib(int n) {
}


extern int uart_count;



void main() {
  /*
   * Machine mode passes us the scratch structure through
   * the thread pointer register. We need to then move it
   * to our sscratch register
   */
  struct rv::scratch *sc = (rv::scratch *)rv::get_tp();
  rv::set_sscratch((rv::xsize_t)sc);


  int hartid = rv::hartid();
  /* TODO: release these somehow :) */
  if (rv::hartid() != 0)
    while (1) arch_halt();


  /* Zero the BSS section */
  for (char *c = _bss_start; c != _bss_end; c++) *c = 0;

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();

  rv::uart_init();
  write_csr(stvec, kernelvec);

  rv::intr_on();


  printk(KERN_DEBUG "Freeing %dMB of ram %llx:%llx\n", CONFIG_RISCV_RAM_MB, _kernel_end, PHYSTOP);

  use_kernel_vm = 1;
  phys::free_range((void *)_kernel_end, (void *)PHYSTOP);

  cpu::seginit(NULL);


  /* Now that we have a memory allocator, call global constructors */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) (*func)();


  /* static fdt, might get eaten by physical allocator. We gotta copy it asap :) */
	dtb::device_tree dt(sc->dtb);

	fib('a');

  // rv::intr_on();
	int *x = NULL;

	x[0] = 0;

  while (1) {
    arch_halt();
  }
}

