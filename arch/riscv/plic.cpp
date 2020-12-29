#include <riscv/plic.h>
#include <riscv/arch.h>
#include <printk.h>

void rv::plic::hart_init(void) {
	/*  */
}

void rv::plic::ack(void) {
	rv::set_sip(rv::get_sip() & ~2);
}
