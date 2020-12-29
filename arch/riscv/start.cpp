#include <arch.h>
#include <console.h>
#include <cpu.h>
#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/uart.h>
#include <riscv/plic.h>
#include <util.h>


typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" void _start(void);
extern "C" char _kernel_end[];
extern "C" char _bss_start[];
extern "C" char _bss_end[];

/* For now, aux harts wait on this variable *and* SIPI */
static volatile int aux_hart_continue = 0;


void main_hart_start() {
  rv::uart_init();


	printk(KERN_DEBUG "Machine ID: %llx\n", RV_READ_CSR(mimpid));
  /* Zero out the BSS in C++ cause doing it in assembly is ugly :^) */
  printk(KERN_DEBUG "Zeroing BSS: [%llx-%llx]\n", _bss_start, _bss_end);
  for (char *c = _bss_start; c != _bss_end; c++) *c = 0;

  use_kernel_vm = true;

  /* Tell the physical allocator about what ram we have */
  printk(KERN_DEBUG "Freeing %dMB of ram %llx:%llx\n", CONFIG_RISCV_RAM_MB, _kernel_end, PHYSTOP);
  phys::free_range((void *)_kernel_end, (void *)PHYSTOP);

  /* Call all the global constructors now that we have a memory allocator */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }
	/* Initialize the platform interrupt controller */
	rv::plic::hart_init();

  printk(KERN_DEBUG "Main hart %zu started.\n", rv::mhartid());

  aux_hart_continue = 1;

  while (1) {
    arch_halt();
  }
}

void aux_hart_start() {
  while (aux_hart_continue == 0) {
    arch_halt();
  }

  printk(KERN_DEBUG "hart %d continuing\n", rv::mhartid());

  while (1) {
    arch_halt();
  }
}


/* main entrypoint for C++ code from assembly */
extern "C" void kstart(void) {
  cpu::seginit(NULL /* no "local" concept on riscv, because we have mhartid */);

	/*
	 * The spec (riscv-privileged-v1.9.1.pdf) specifies that all riscv implementations
	 * must provide read access to the Hart ID Register, and that at least one hart must
	 * have a Hart ID of zero. Here we take advantage of that and consider it to be the
	 * "boot thread" which we initialize stuff from.
	 */
  if (rv::mhartid() == 0) {
    main_hart_start();
  } else {
    aux_hart_start();
  }
}
