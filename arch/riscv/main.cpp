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

int fib(int n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
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

  // rv::intr_on();

  while (1) {
    arch_halt();
  }
}


struct ktrapframe {
  rv::xsize_t ra; /* x1: Return address */
  rv::xsize_t sp; /* x2: Stack pointer */
  rv::xsize_t gp; /* x3: Global pointer */
  rv::xsize_t tp; /* x4: Thread Pointer */
  rv::xsize_t t0; /* x5: Temp 0 */
  rv::xsize_t t1; /* x6: Temp 1 */
  rv::xsize_t t2; /* x7: Temp 2 */
  rv::xsize_t s0; /* x8: Saved register / Frame Pointer */
  rv::xsize_t s1; /* x9: Saved register */
  rv::xsize_t a0; /* Arguments, you get it :) */
  rv::xsize_t a1;
  rv::xsize_t a2;
  rv::xsize_t a3;
  rv::xsize_t a4;
  rv::xsize_t a5;
  rv::xsize_t a6;
  rv::xsize_t a7;
  rv::xsize_t s2; /* More Saved registers... */
  rv::xsize_t s3;
  rv::xsize_t s4;
  rv::xsize_t s5;
  rv::xsize_t s6;
  rv::xsize_t s7;
  rv::xsize_t s8;
  rv::xsize_t s9;
  rv::xsize_t s10;
  rv::xsize_t s11;
  rv::xsize_t t3; /* More temporaries */
  rv::xsize_t t4;
  rv::xsize_t t5;
  rv::xsize_t t6;
  /* Missing floating point registers in the kernel trap frame */
};


// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
extern "C" void kerneltrap(struct ktrapframe &tf) {
  int which_dev = 0;
  rv::xsize_t sepc = read_csr(sepc);
  rv::xsize_t sstatus = read_csr(sstatus);
  rv::xsize_t scause = read_csr(scause);

  if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
  if (rv::intr_enabled() != 0) panic("kerneltrap: interrupts enabled");

  int interrupt = (scause >> 63);
  int nr = scause & ~(1llu << 63);
  if (interrupt) {
    /* Supervisor software interrupt (from machine mode) */
    if (nr == 1) {
      // acknowledge the software interrupt by clearing
      // the SSIP bit in sip.
      write_csr(sip, read_csr(sip) & ~2);
    }
    /* Supervisor External Interrupt */
    if (nr == 9) {
      int irq = rv::plic::claim();
      // printk("irq: %d\n", irq);
      irq::dispatch(irq, NULL /* Hmm, not sure what to do with regs */);
      rv::plic::complete(irq);
    }
  } else {
  }

  /* restore these regs in case other code causes traps */
  write_csr(sepc, sepc);
  write_csr(sstatus, sstatus);
}
