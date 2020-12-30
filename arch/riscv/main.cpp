#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/uart.h>

extern "C" char _kernel_end[];

volatile long count = 0;

/* lowlevel.S, calls kerneltrap() */
extern "C" void kernelvec(void);

void main() {

	int x = 0;
	struct rv::scratch *sc = (rv::scratch*)rv::get_tp();
	printk("hart: %d\n", sc->hartid);

  printk(KERN_DEBUG "Freeing %dMB of ram %llx:%llx\n", CONFIG_RISCV_RAM_MB, _kernel_end, PHYSTOP);
  phys::free_range((void *)_kernel_end, (void *)PHYSTOP);

  write_csr(stvec, kernelvec);

	rv::intr_on();
  // rv::intr_off();

  while (1) {
		printk("count: %d\n", count);
		arch_halt();
  }
}



// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr() {
  rv::xsize_t scause = read_csr(scause);

  if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
#if 0
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if (irq == UART0_IRQ) {
      uartintr();
    } else if (irq == VIRTIO0_IRQ) {
      virtio_disk_intr();
    } else if (irq) {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq) plic_complete(irq);
#endif

    return 1;
  } else if (scause == 0x8000000000000001L) {
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

#if 0
    if (cpuid() == 0) {
      clockintr();
    }
#endif

    count += 1;

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    write_csr(sip, read_csr(sip) & ~2);

    return 2;
  } else {
    return 0;
  }
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
extern "C" void kerneltrap() {
  int which_dev = 0;
  rv::xsize_t sepc = read_csr(sepc);
  rv::xsize_t sstatus = read_csr(sstatus);
  rv::xsize_t scause = read_csr(scause);

  if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
  if (rv::intr_enabled() != 0) panic("kerneltrap: interrupts enabled");

  /* if the top bit is set,  */

  if ((which_dev = devintr()) == 0) {
    printk("scause %p\n", scause);
    printk("sepc=%p stval=%p\n", read_csr(sepc), read_csr(stval));
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  // if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) yield();

  /* restore these regs in case other code causes traps */
  write_csr(sepc, sepc);
  write_csr(sstatus, sstatus);
}
