#include <riscv/uart.h>
#include <printk.h>
#include <arch.h>
#include <console.h>

float square(float x) {
	return x * x;
}

extern "C" void kmain(void) {
	rv::uart_init();
	printk("Hello, world\n");
  while (1) {
  }
}
