#include <module.h>
#include <dev/8250-uart.h>
#include <dev/driver.h>
#include <dev/KernelLogger.h>
#include <console.h>

#define LOG(...) PFXLOG(YEL "ns16550a", __VA_ARGS__)

static void uart_interrupt_handle(int irq, reg_t *regs, void *uart);


#include <riscv/arch.h>

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

#define DLL 0  // divisor latch low
#define DLM 1  // divisor latch high
#define RBR 0  // read data
#define THR 0  // write data
#define IER 1  // interrupt enable
#define IIR 2  // interrupt identify (read)
#define FCR 2  // FIFO control (write)
#define LCR 3  // line control
#define MCR 4  // modem control
#define LSR 5  // line status
#define MSR 6  // modem status
#define SCR 7  // scratch

#define USE_FIFOS 0

namespace ns16550a {


  class Uart : public dev::Device, public KernelLogger {
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

    using dev::Device::Device;

    void init() override {
      auto mmio = dev()->cast<hw::MMIODevice>();

      this->mmio = true;
      regs = (uint8_t *)p2v(mmio->address());


      write_reg(SCR, 0xde);
      if (read_reg(SCR) != 0xde) {
        return;
      }



      // line control register
      // set DLAB so we can write divisor
      write_reg(LCR, 0x80);

      // write divisor to divisor latch to set speed
      // LSB then MSB
      // 115200 / 1 = 115200 baud
      write_reg(DLL, 1);
      write_reg(DLM, 0);

      // line control register
      // turn DLAB off, set
      // 8 bit word, 1 stop bit, no parity
      write_reg(LCR, 0x03);

      // interrupt enable register
      // raise interrupts on received data available
      // start off not getting interrupt on transmit hoilding register empty
      // this is turned on/off in kick_output()
      // ignore line status update and modem status update
      write_reg(IER, 0x01);

#if !USE_FIFOS
      // FIFO control register
      // turn off FIFOs;  chip is now going to raise an
      // interrupt on every incoming word
      write_reg(FCR, 0);
#else
      // FIFO control register
      // turn on FIFOs;  chip is now going to raise an
      // interrupt on every 14 bytes or when 4 character
      // times have passed without getting a read despite there
      // being a character available
      // 1100 0001
      write_reg(FCR, 0xc1);
#endif

      handle_irq(mmio->interrupt, "ns16550a");
      arch::irq::enable(mmio->interrupt);


      // Now that we have initialized, register with the KernelLogger subsystem so
      // this device is used whenever the kernel wants to print something (/dev/console)
      register_logger();
    }

    int putc(int c) override {
      nputc++;
      while (!(read_reg(LSR) & UART_LSR_THRE)) {
      }
      write_reg(RBR, c);
      return 0;
    }

    int get_char(bool wait = true) {
      while ((read_reg(LSR) & UART_LSR_DR) == 0) {
        if (wait) {
          arch_relax();
        } else {
          return -1;
        }
      }
      return read_reg(RBR);
    }

    void write_reg(uint8_t offset, uint8_t val) {
      if (mmio) {
        *(volatile uint8_t *)(regs + offset) = val;
      } else {
#ifdef CONFIG_X86
        outb(val, (uint16_t)((off_t)regs + offset));
#endif
      }
    }

    uint8_t read_reg(uint8_t offset) {
      if (mmio) {
        return *(volatile uint8_t *)(regs + offset);
      } else {
#ifdef CONFIG_X86
        return inb((uint16_t)((off_t)regs + offset));
#endif
      }
      return 0;
    }


    void kick_input(void) {
      uint64_t count = 0;

      while (1) {
        uint8_t ls = read_reg(LSR);
        if (ls & 0x04) {
          // parity error, skip this byte
          continue;
        }
        if (ls & 0x08) {
          // framing error, skip this byte
          continue;
        }
        if (ls & 0x10) {
          // break interrupt, but we do want this byte
        }
        if (ls & 0x02) {
          // overrun error - we have lost a byte
          // but we do want this next one
        }
        if (ls & 0x01) {
          // data ready
          // grab a byte from the device if there is room
          char data = read_reg(RBR);
          console::feed(1, &data);
          count++;
        } else {
          // chip is empty, stop receiving from it
          break;
        }
      }
    }


    void kick_output(void) {
#if 0
      uint64_t count = 0;

      while (!serial_output_empty(s)) {
        uint8_t ls = read_reg(LSR);
        if (ls & 0x20) {
          // transmit holding register is empty
          // drive a byte to the device
          uint8_t data = serial_output_pull(s);
          serial_write_reg(s, THR, data);
          // chip is full, stop sending to it
          // but since we have more data, have it
          // interrupt us when it has room
          uint8_t ier = serial_read_reg(s, IER);
          ier |= 0x2;
          serial_write_reg(s, IER, ier);
					return;
        }
      }

      // the chip has room, but we have no data for it, so
      // disable the transmit interrupt for now
      uint8_t ier = serial_read_reg(s, IER);
      ier &= ~0x2;
      serial_write_reg(s, IER, ier);
    out:
#endif
      return;
    }


    void irq(int nr) override {
      uint8_t iir;
      int done = 0;
      do {
        iir = read_reg(IIR);

        switch (iir & 0b1111) {
          case 0:  // modem status reset + ignore
            (void)read_reg(MSR);
            break;
          case 2:  // THR empty (can send more data)
            outputlock.lock();
            kick_output();
            outputlock.unlock();
            break;
          case 4:   // received data available
          case 12:  // received data available (FIFO timeout)
            inputlock.lock();
            kick_input();
            inputlock.unlock();
            break;
          case 6:  // line status reset + ignore
            (void)read_reg(LSR);
            break;
          case 1:  // done
            outputlock.lock();
            kick_output();
            outputlock.unlock();

            inputlock.lock();
            kick_input();
            inputlock.unlock();
            break;
          default:  // wtf
            break;
        }

      } while ((iir & 0xf) != 1);
    }


    bool mmio = true;
    spinlock inputlock;
    spinlock outputlock;
    uint8_t *regs;
    uint64_t nputc = 0;
  };

}  // namespace ns16550a



static dev::ProbeResult probe(ck::ref<hw::Device> dev) {
  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->is_compat("ns16550a")) {
      LOG("Found device @%08llx. irq=%d\n", mmio->address(), mmio->interrupt);
      return dev::ProbeResult::Attach;
    }
  }

  return dev::ProbeResult::Ignore;
};

driver_init("ns16550a", ns16550a::Uart, probe);
