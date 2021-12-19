// Much of this driver was adapted from u-boot

#include <module.h>
#include <devicetree.h>
#include <printk.h>
#include <dev/driver.h>
#include <dev/device.h>
#include <device_majors.h>
#include <dev/sifive/uart.h>
#include <console.h>
#include <riscv/arch.h>
#include <riscv/plic.h>


#define UART_TXFIFO_FULL 0x80000000
#define UART_RXFIFO_EMPTY 0x80000000
#define UART_RXFIFO_DATA 0x000000ff
#define UART_TXCTRL_TXEN 0x1
#define UART_RXCTRL_RXEN 0x1

/* IP register */
#define UART_IP_RXWM 0x2


#define LOG(...) PFXLOG(BLU "sifive-uart", __VA_ARGS__)


/**
 * Find minimum divisor divides in_freq to max_target_hz;
 * Based on uart driver n SiFive FSBL.
 *
 * f_baud = f_in / (div + 1) => div = (f_in / f_baud) - 1
 * The nearest integer solution requires rounding up as to not exceed
 * max_target_hz.
 * div  = ceil(f_in / f_baud) - 1
 *	= floor((f_in - 1 + f_baud) / f_baud) - 1
 * This should not overflow as long as (f_in - 1 + f_baud) does not exceed
 * 2^32 - 1, which is unlikely since we represent frequencies in kHz.
 */
static inline unsigned int uart_min_clk_divisor(unsigned long in_freq, unsigned long max_target_hz) {
  unsigned long quotient = (in_freq + max_target_hz - 1) / (max_target_hz);
  /* Avoid underflow */
  if (quotient == 0)
    return 0;
  else
    return quotient - 1;
}


void sifive_uart_interrupt_handle(int irq, reg_t *regs, void *uart) {
  auto *u = (sifive::Uart *)uart;
  u->handle_irq();
}

sifive::Uart::Uart(dev::MMIODevice &mmio) {
  // the registers are simply at the base
  regs = (Uart::Regs *)p2v(mmio.address());
  // enable transmit and receive
  regs->txctrl = UART_TXCTRL_TXEN;
  regs->rxctrl = UART_RXCTRL_RXEN;

  // enable rx interrupt
  regs->ie = 0b10;

  irq::install(mmio.interrupt, sifive_uart_interrupt_handle, "sifive,uart0", (void *)this);
}

void sifive::Uart::put_char(char c) {
  int iters = 0;
  while (regs->txfifo & UART_TXFIFO_FULL) {
  }

  regs->txfifo = c;
  // printk("iters = %d\n", iters);
}


int sifive::Uart::get_char(bool wait) {
  while (1) {
    uint32_t r = regs->rxfifo;

    if (r & UART_RXFIFO_EMPTY) {
      if (!wait) {
        return -1;
      }
      continue;
    }

    return r & 0xFF;
  }
  return -1;
}

void sifive::Uart::setbrg(unsigned long clock, unsigned long baud) {
  // set the divider for the clock
  regs->div = uart_min_clk_divisor(clock, baud);
}



void sifive::Uart::handle_irq(void) {
  while (1) {
    uint32_t r = regs->rxfifo;
    if (r & UART_RXFIFO_EMPTY) {
      break;
    }
    char buf = r & 0xFF;
    console::feed(1, &buf);
  }
}


dev::ProbeResult sifive::UartDriver::probe(ck::ref<dev::Device> dev) {
  if (auto mmio = dev->cast<dev::MMIODevice>()) {
    if (mmio->is_compat("sifive,uart0")) {
      LOG("Found device @%08llx. irq=%d\n", mmio->address(), mmio->interrupt);

      auto uart = ck::make_box<sifive::Uart>(*mmio);
      uarts.push(move(uart));
      return dev::ProbeResult::Attach;
    }
  }
  return dev::ProbeResult::Ignore;
};

driver_init("sifive,uart0", sifive::UartDriver);
