#include <riscv/plic.h>
#include <riscv/arch.h>
#include <riscv/memlayout.h>
#include <printk.h>

void rv::plic::hart_init(void) {
	int hart = rv::hartid();
	/* Clear the "supervisor enable" field. This is the register that enables or disables
	 * external interrupts (UART, DISK, ETC) */
	PLIC_SENABLE(hart) = 0;

	/* set this hart's S-mode priority threshold to 0. */
  PLIC_SPRIORITY(hart) = 0;
}


int rv::plic::claim(void) {
	int hart = rv::hartid();
	int irq = PLIC_SCLAIM(hart);
  return irq;
}

void rv::plic::complete(int irq) {
	int hart = rv::hartid();
	PLIC_SCLAIM(hart) = irq;
}

void rv::plic::enable(int irq, int priority) {
	/* Set the priority register */
	MREG(PLIC + irq * 4) = priority;
	/* Enable the irq in the SENABLE register */
	PLIC_SENABLE(rv::hartid()) |= (1 << irq);
}

void rv::plic::disable(int irq) {
	/* Disable the irq in the SENABLE register */
	PLIC_SENABLE(rv::hartid()) &= ~(1 << irq);
}
