#include <module.h>
#include <devicetree.h>
#include <printk.h>
#include <dev/driver.h>
#include <device_majors.h>


// UARTSifiveRegs.ie, ip
enum {
  kUartSifiveTxwm = 1 << 0,
  kUartSifiveRxwm = 1 << 1,
};


struct UARTSifiveRegs {
  union Txdata {
    struct {
      uint32_t data : 8;
      uint32_t reserved : 23;
      uint32_t isFull : 1;
    };
    uint32_t val;
  } txdata;

  union Rxdata {
    struct {
      uint32_t data : 8;
      uint32_t reserved : 23;
      uint32_t isEmpty : 1;
    };
    uint32_t val;
  } rxdata;

  union Txctrl {
    struct {
      uint32_t enable : 1;
      uint32_t nstop : 1;
      uint32_t reserved1 : 14;
      uint32_t cnt : 3;
      uint32_t reserved2 : 13;
    };
    uint32_t val;
  } txctrl;

  union Rxctrl {
    struct {
      uint32_t enable : 1;
      uint32_t reserved1 : 15;
      uint32_t cnt : 3;
      uint32_t reserved2 : 13;
    };
    uint32_t val;
  } rxctrl;

  uint32_t ie;  // interrupt enable
  uint32_t ip;  // interrupt pending
  uint32_t div;
  uint32_t unused;
};


/*
 * Config macros
 */

/*
 * SIFIVE_SERIAL_MAX_PORTS: maximum number of UARTs on a device that can
 *                          host a serial console
 */
#define SIFIVE_SERIAL_MAX_PORTS 8

/*
 * SIFIVE_DEFAULT_BAUD_RATE: default baud rate that the driver should
 *                           configure itself to use
 */
#define SIFIVE_DEFAULT_BAUD_RATE 115200

/* SIFIVE_SERIAL_NAME: our driver's name that we pass to the operating system */
#define SIFIVE_SERIAL_NAME "sifive-serial"

/* SIFIVE_TTY_PREFIX: tty name prefix for SiFive serial ports */
#define SIFIVE_TTY_PREFIX "ttySIF"

/* SIFIVE_TX_FIFO_DEPTH: depth of the TX FIFO (in bytes) */
#define SIFIVE_TX_FIFO_DEPTH 8

/* SIFIVE_RX_FIFO_DEPTH: depth of the TX FIFO (in bytes) */
#define SIFIVE_RX_FIFO_DEPTH 8


class SifiveUart {
 public:
  SifiveUart(addr_t base, int64_t clock) : base(base), clock(clock) {
    uint32_t baud = 115200;
    uint64_t quotient = (clock + baud - 1) / baud;

    if (quotient == 0)
      regs()->div = 0;
    else
      regs()->div = (uint32_t)(quotient - 1);
  }

  void put_char(char ch) {
    while (regs()->txdata.isFull) {
    }
    regs()->txdata.val = ch;
  }


  int get_char(bool wait = true) {
    UARTSifiveRegs::Rxdata data;
    do {
      data.val = regs()->rxdata.val;
    } while (!wait || data.isEmpty);

    return data.isEmpty ? -1 : data.data;
  }

  inline UARTSifiveRegs *regs() { return (UARTSifiveRegs *)base; }

 private:
  addr_t base;
  int64_t clock;
};


struct fs::FileOperations sifive_uart_ops = {
    // .read = sifive_uart_read,
    // .write = sifive_uart_write,
    // .ioctl = sifive_uart_ioctl,
    // .open = sifive_uart_open,
    // .close = sifive_uart_close,
    // .poll = sifive_uart_poll,
};

static struct dev::DriverInfo sifive_uart_driver_info {
  .name = SIFIVE_SERIAL_NAME, .type = DRIVER_CHAR, .major = MAJOR_SIFIVE_UART,

  .char_ops = &sifive_uart_ops,
};


void init_device(dtb::node *node) {
  // create a uart device
  SifiveUart uart(node->address, 5);

  uart.put_char('a');

  // while (1) {
  //   int x = uart.get_char(false);
  //   printk("%d\n", x);
  //   if (x == -1) {
  //     break;
  //   }
  //   printk("%02x: %c\n", x, x);
  // }

  KINFO("sifive,uart0 at %p\n", node->address);
}




void sifive_uart_init(void) {
  dtb::walk_devices([](dtb::node *node) -> bool {
    // printk("Compat: %s\n", node->compatible);
    for (int i = 0; i < node->ncompat; i++) {
      if (!strcmp(node->compatible[i], "sifive,uart0")) {
        init_device(node);
      }
    }

    return true;
  });
}
module_init("sifive,uart0", sifive_uart_init);
