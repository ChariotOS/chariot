#include <riscv/memlayout.h>


namespace rv {
	void uart_init(void);

	void uart_putc(char c);
	char uart_getc(void);
}  // namespace rv
