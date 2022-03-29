#include <arm64/gic.h>
#include <printf.h>
#include <reg.h>
#include <types.h>

/* TODO: parse this from the DTB that arm gives us */
#define UART_BASE (0x9000000)

#define ARM_GENERIC_TIMER_VIRTUAL_INT 27
#define ARM_GENERIC_TIMER_PHYSICAL_INT 30
#define UART0_INT (32 + 1)
#define VIRTIO0_INT (32 + 16)

#define MAX_INT 128

/* PL011 implementation */
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

#define NUM_UART 1
#define UARTREG(base, reg) (*REG32((base) + (reg)))


static inline uintptr_t uart_to_ptr(unsigned int n) {
  switch (n) {
    default:
    case 0:
      return UART_BASE;
  }
}


static void uart_irq(int irq, reg_t *r, void *data) {}

/* Init the PrimeCell PL011 UART */
void setup_pl011(void) {
  for (size_t i = 0; i < NUM_UART; i++) {
    UARTREG(uart_to_ptr(i), UART_CR) = (1 << 8) | (1 << 0);  // tx_enable, uarten
  }
  return;

  for (size_t i = 0; i < 1; i++) {
    uintptr_t base = uart_to_ptr(i);

    // create circular buffer to hold received data
    // cbuf_initialize(&uart_rx_buf[i], RXBUF_SIZE);

    // assumes interrupts are contiguous
    // register_int_handler(UART0_INT + i, &uart_irq, (void *)i);

    // clear all irqs
    UARTREG(base, UART_ICR) = 0x3ff;

    // set fifo trigger level
    UARTREG(base, UART_IFLS) = 0;  // 1/8 rxfifo, 1/8 txfifo

    // enable rx interrupt
    UARTREG(base, UART_IMSC) = (1 << 4);  // rxim

    // enable receive
    UARTREG(base, UART_CR) = UARTREG(base, UART_CR) | (1 << 9);  // rxen

    // enable interrupt
    // irq::install(UART0_INT + i, uart_irq, "Primecell PL011 UART", (void *)i);
  }
}


void serial_install();
int serial_rcvd(int device) { return 0; }
char serial_recv(int device) { return 0; }
char serial_recv_async(int device) { return 0; }
int serial_transmit_empty(int device) { return 0; }


void serial_send(int device, char c) {
  uintptr_t base = uart_to_ptr(device);

  /* spin while fifo is full */
  while (UARTREG(base, UART_TFR) & (1 << 5))
    ;
  UARTREG(base, UART_DR) = c;
}


void serial_string(int device, char *out) {}


void arm64_platform_init(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3) {
  /* Initialize the interrupt controller */
  arm64::gic_init();

  setup_pl011();

  printf("It worked!\n");

  while (1) {
  }
}
