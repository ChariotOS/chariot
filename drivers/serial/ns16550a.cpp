#include <module.h>
#include <dev/8250-uart.h>
#include <dev/driver.h>

#define LOG(...) PFXLOG(YEL "ns16550a", __VA_ARGS__)

static void uart_interrupt_handle(int irq, reg_t *regs, void *uart);


#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

namespace ns16550a {


  class Uart {
   public:
    Uart(dev::MMIODevice &mmio) {
      address = mmio.address();

      irq::install(mmio.interrupt, uart_interrupt_handle, "ns16550a", (void *)this);

      /* Master interrupt enable; also keep DTR/RTS asserted. */
      write_reg(UART_MCR, UART_MCR_OUT2 | UART_MCR_DTR | UART_MCR_RTS);

      /* Enable receive interrupts. */
      write_reg(UART_IER, UART_IER_ERDAI);

			put_char('a');
    }


    void wait_for_xmitr(void) {
      unsigned int status;
      for (;;) {
        status = read_reg(UART_LSR);
        if ((status & BOTH_EMPTY) == BOTH_EMPTY) return;
        arch_relax();
      }
    }

    void put_char(char c) {
			wait_for_xmitr();
			write_reg(UART_THR, c);
		}
    int get_char(bool wait = true) { return -1; }

    void handle_irq() {
      //
      printk("handle irq\n");
    }

    void write_reg(uint32_t off, uint8_t val) {
      *(volatile uint8_t *)p2v(address + off) = val;
    }

    uint32_t read_reg(uint8_t off) { return *(volatile uint8_t *)p2v(address + off); }

    off_t address;
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
