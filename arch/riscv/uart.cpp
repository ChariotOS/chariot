#include <arch.h>
#include <mem.h>
#include <console.h>
#include <lock.h>
#include <mem.h>
#include <printk.h>
#include <riscv/uart.h>
#include <riscv/sbi.h>

/* Quick function to get a uart register */
// #define Reg(reg) ((volatile unsigned char *)(p2v((UART0 + reg))))
#define Reg(reg) ((volatile unsigned char *)((((off_t)p2v(UART0) + reg))))

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define RHR 0  // receive holding register (for input bytes)
#define THR 0  // transmit holding register (for output bytes)
#define IER 1  // interrupt enable register
#define IER_RX_ENABLE (1 << 0)
#define IER_TX_ENABLE (1 << 1)
#define FCR 2  // FIFO control register
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)  // clear the content of the two FIFOs
#define ISR 2                    // interrupt status register
#define LCR 3                    // line control register
#define LCR_EIGHT_BITS (3 << 0)
#define LCR_BAUD_LATCH (1 << 7)  // special mode to set baud rate
#define LSR 5                    // line status register
#define LSR_RX_READY (1 << 0)    // input is waiting to be read from RHR
#define LSR_TX_IDLE (1 << 5)     // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))


// the transmit output buffer.
// struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 8
static spinlock uart_tx_lock;
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64_t uart_tx_w;  // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64_t uart_tx_r;  // read next from uart_tx_buf[uar_tx_r % UART_TX_BUF_SIZE]

int uart_count = 0;

static void uart_start(void) {
  /* if we have sbi, there's no reason to do all this stuff :) */
  while (1) {
    if (uart_tx_w == uart_tx_r) {
      // transmit buffer is empty.
      return;
    }

    if ((ReadReg(LSR) & LSR_TX_IDLE) == 0) {
      // the UART transmit holding register is full,
      // so we cannot give it another byte.
      // it will interrupt when it's ready for a new byte.
      return;
    }

    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;

    // maybe uartputc() is waiting for space in the buffer.
    // wakeup(&uart_tx_r);

    WriteReg(THR, c);
  }
}


static void uart_irq(int irq, reg_t *r, void *data) {

	// printk_nolock("irq\n");
  uart_count++;

  size_t nread = 0;
  char buf[32];
  while (1) {
    int c = rv::uart_getc();
    if (c == -1) break;
    /* qemu's UART is only sending \r when I hit enter... */
    if (c == '\r') c = '\n';

    if (nread > 32) {
      console::feed(nread, buf);
      nread = 0;
    }

    buf[nread] = c;
    nread++;
  }

  if (nread != 0) console::feed(nread, buf);

  // send buffered characters.
  uart_tx_lock.lock();
  uart_start();
  uart_tx_lock.unlock();
}



void rv::uart_init(void) {
  uart_tx_w = uart_tx_r = 0;
  // disable interrupts.
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  WriteReg(LCR, LCR_BAUD_LATCH);

  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);

  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  irq::install(UART0_IRQ, uart_irq, "riscv uart");
  // enable transmit and receive interrupts.
  WriteReg(IER, IER_RX_ENABLE);
}

void rv::uart_putc(char c) {
  /* If we have SBI, ask it to print things. This might be a little slower, but it should work okay
   */
  sbi_call(SBI_CONSOLE_PUTCHAR, c);
  return;
  // wait for Transmit Holding Empty to be set in LSR.
  while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);
  return;

  // WriteReg(THR /* Transmit holding register */, c);

  while (1) {
    uart_tx_lock.lock();
    if (uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE) {
      uart_tx_lock.unlock();
      continue;
    } else {
      uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
      uart_tx_w += 1;
      uart_start();
      uart_tx_lock.unlock();
      break;
    }
  }
}

int rv::uart_getc(void) {
  if (ReadReg(LSR) & 0x01) {
    // input data is ready.
    return ReadReg(RHR) & 0xFF;
  } else {
    return -1;
  }
}


void serial_install() { rv::uart_init(); }


int serial_rcvd(int device) { return 0; }
char serial_recv(int device) { return 0; }
char serial_recv_async(int device) { return 0; }

int serial_transmit_empty(int device) { return 0; }

void serial_send(int device, char out) {
  /* We only support uart 1 */
  if (device != 1 /* uart0, I guess */) {
    // return;
  }
  /* TODO: this needs to be efficient and wait on interrupts (real hardware lol) */
  rv::uart_putc(out);
}

void serial_string(int device, char *out) {
  if (device != 1 /* uart0, I guess */) {
    return;
  }

  for (int i = 0; out[i] != '\0'; i++) {
    rv::uart_putc(out[i]);
  }
}
