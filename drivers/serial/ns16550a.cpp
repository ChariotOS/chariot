#include <module.h>
#include <dev/8250-uart.h>
#include <dev/driver.h>

#define LOG(...) PFXLOG(YEL "ns16550a", __VA_ARGS__)

static void uart_interrupt_handle(int irq, reg_t *regs, void *uart);


#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

namespace ns16550a {


  class Uart {
   public:
    struct Regs {
      volatile uint8_t rbr;  /* 0 */
      volatile uint8_t ier;  /* 1 */
      volatile uint8_t fcr;  /* 2 */
      volatile uint8_t lcr;  /* 3 */
      volatile uint8_t mcr;  /* 4 */
      volatile uint8_t lsr;  /* 5 */
      volatile uint8_t msr;  /* 6 */
      volatile uint8_t spr;  /* 7 */
      volatile uint8_t mdr1; /* 8 */
      volatile uint8_t reg9; /* 9 */
      volatile uint8_t regA; /* A */
      volatile uint8_t regB; /* B */
      volatile uint8_t regC; /* C */
      volatile uint8_t regD; /* D */
      volatile uint8_t regE; /* E */
      volatile uint8_t uasr; /* F */
      volatile uint8_t scr;  /* 10*/
      volatile uint8_t ssr;  /* 11*/
    };
    Uart(dev::MMIODevice &mmio) {
      regs = (Uart::Regs *)p2v(mmio.address());

      irq::install(mmio.interrupt, uart_interrupt_handle, "ns16550a", (void *)this);

      // set DLAB so we can write the divisor
      regs->lcr = 0x80;

      regs->rbr = 1;
      regs->ier = 0;
      regs->lcr = 0x03;

      /* Enable receive interrupts. */
      regs->ier = 0x01;
      /* disable fifos, an irq will be raised for each incomming word */
      regs->fcr = 0;

      (void)regs->rbr;

      __sync_synchronize();

      putc('a');
    }

    void putc(char c) {
      while (!(regs->lsr & UART_LSR_THRE)) {
      }
      regs->rbr = c;
    }

    int get_char(bool wait = true) {
      while ((regs->lsr & UART_LSR_DR) == 0) {
        if (wait) {
          arch_relax();
        } else {
          return -1;
        }
      }
      return regs->rbr;
    }

    void handle_irq() {}


    Uart::Regs *regs;
  };


  class UartDriver : public dev::Driver {
   public:
    ck::vec<ck::box<ns16550a::Uart>> uarts;
    ~UartDriver(void);
    dev::ProbeResult probe(ck::ref<dev::Device> dev);
  };

  UartDriver::~UartDriver(void) {}


  dev::ProbeResult UartDriver::probe(ck::ref<dev::Device> dev) {
    if (auto mmio = dev->cast<dev::MMIODevice>()) {
      if (mmio->is_compat("ns16550a")) {
        LOG("Found device @%08llx. irq=%d\n", mmio->address(), mmio->interrupt);
        auto uart = ck::make_box<ns16550a::Uart>(*mmio);
        uarts.push(move(uart));
        return dev::ProbeResult::Attach;
      }
    }

    return dev::ProbeResult::Ignore;
  };

}  // namespace ns16550a




void uart_interrupt_handle(int irq, reg_t *regs, void *uart) {
  auto *u = (ns16550a::Uart *)uart;
  u->handle_irq();
}  // namespace ns16550a


driver_init("ns16550a", ns16550a::UartDriver);
