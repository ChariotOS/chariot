#include <arch.h>
#include <platform/bcm711.h>
#include <reg.h>
#include <types.h>

/* This is a PL011 Driver (The one on the bcm28XX family) */
/* TODO: make this a normal driver somewhere */
#define UART_DR (0x00)
#define UART_RSR (0x04)
#define UART_TFR (0x18)
#define UART_ILPR (0x20)
#define UART_IBRD (0x24)
#define UART_FBRD (0x28)
#define UART_LCRH (0x2c)
#define UART_CR (0x30)
#define UART_IFLS (0x34)
#define UART_IMSC (0x38)
#define UART_TRIS (0x3c)
#define UART_TMIS (0x40)
#define UART_ICR (0x44)
#define UART_DMACR (0x48)

#define UARTREG(base, reg) (*REG32((base) + (reg)))

#define RXBUF_SIZE 16
#define NUM_UART 1

static inline uintptr_t uart_to_ptr(unsigned int n) {
  switch (n) {
    default:
    case 0:
      return UART0_BASE;
  }
}

/* Early serial initialization */
void serial_install() {
  for (size_t i = 0; i < NUM_UART; i++) {
    UARTREG(uart_to_ptr(i), UART_CR) = (1 << 8) | (1 << 0);  // tx_enable, uarten
  }
  return;
  for (size_t i = 0; i < NUM_UART; i++) {
    // create circular buffer to hold received data
    // cbuf_initialize(&uart_rx_buf[i], RXBUF_SIZE);

    // assumes interrupts are contiguous
    // register_int_handler(INTERRUPT_VC_UART + i, &uart_irq, (void *)i);

    // clear all irqs
    UARTREG(uart_to_ptr(i), UART_ICR) = 0x3ff;

    // set fifo trigger level
    UARTREG(uart_to_ptr(i), UART_IFLS) = 0;  // 1/8 rxfifo, 1/8 txfifo

    // enable rx interrupt
    UARTREG(uart_to_ptr(i), UART_IMSC) = (1 << 6) | (1 << 4);  // rtim, rxim

    // enable receive
    UARTREG(uart_to_ptr(i), UART_CR) |= (1 << 9);  // rxen

    // enable interrupt
    // unmask_interrupt(INTERRUPT_VC_UART + i);
  }
}


void serial_send(int port, char c) {
  uintptr_t base = uart_to_ptr(port);

  /* spin while fifo is full */
  while (UARTREG(base, UART_TFR) & (1 << 5))
    ;
  UARTREG(base, UART_DR) = c;
}

void serial_string(int port, char *out) {
  for (uint32_t i = 0; out[i] != '\0'; i++) {
    serial_send(port, out[i]);
  }
}
