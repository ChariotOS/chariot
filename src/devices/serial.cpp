
#include <mobo/dev_mgr.h>

#include <stdint.h>
#include <mutex>

#include <linux/serial_reg.h>
#include <linux/types.h>

template <typename T>
static inline T ioport_read(void *dst) {
  return *reinterpret_cast<T *>(dst);
}

template <typename T>
static inline void ioport_write(void *dst, T val) {
  *reinterpret_cast<T *>(dst) = val;
}

/*
 * This fakes a U6_16550A. The fifo len needs to be 64 as the kernel
 * expects that for autodetection.
 */
#define FIFO_LEN 64
#define FIFO_MASK (FIFO_LEN - 1)

#define UART_IIR_TYPE_BITS 0xc0

struct serial8250_device {
  std::mutex mutex;
  uint8_t id;
  uint16_t iobase;
  uint8_t irq;
  uint8_t irq_state;
  int txcnt;
  int rxcnt;
  int rxdone;
  char txbuf[FIFO_LEN];
  char rxbuf[FIFO_LEN];

  uint8_t dll;
  uint8_t dlm;
  uint8_t iir;
  uint8_t ier;
  uint8_t fcr;
  uint8_t lcr;
  uint8_t lsr;
  uint8_t msr;
  uint8_t scr;
  uint8_t mcr;
};

#define SERIAL_REGS_SETTING                                     \
  .iir = UART_IIR_NO_INT, .lsr = UART_LSR_TEMT | UART_LSR_THRE, \
  .msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS, .mcr = UART_MCR_OUT2,

#define DEVICE_COUNT 4

// Some initial devices, tty 0-3
static struct serial8250_device devices[DEVICE_COUNT] = {
    /* ttyS0 */
    [0] = {.id = 0, .iobase = 0x3f8, .irq = 4, SERIAL_REGS_SETTING},
    /* ttyS1 */
    [1] = {.id = 1, .iobase = 0x2f8, .irq = 3, SERIAL_REGS_SETTING},
    /* ttyS2 */
    [2] = {.id = 2, .iobase = 0x3e8, .irq = 4, SERIAL_REGS_SETTING},
    /* ttyS3 */
    [3] = {.id = 3, .iobase = 0x2e8, .irq = 3, SERIAL_REGS_SETTING},
};

static void serial8250_flush_tx(struct serial8250_device *dev) {
  dev->lsr |= UART_LSR_TEMT | UART_LSR_THRE;
  if (dev->txcnt) {
    // TODO: do more than just printf here.
    for (int i = 0; i < dev->txcnt; i++) {
      printf("%c", dev->txbuf[i]);
    }
    dev->txcnt = 0;
  }
}




static void serial8250_rx(struct serial8250_device *dev, void *data)
{
	if (dev->rxdone == dev->rxcnt)
		return;

	/* Break issued ? */
	if (dev->lsr & UART_LSR_BI) {
		dev->lsr &= ~UART_LSR_BI;
		ioport_write<char>(data, 0);
		return;
	}

	ioport_write<char>(data, dev->rxbuf[dev->rxdone++]);
	if (dev->rxcnt == dev->rxdone) {
		dev->lsr &= ~UART_LSR_DR;
		dev->rxcnt = dev->rxdone = 0;
	}
}

static int serial_in(mobo::port_t port, void *data, int sz, void *_dp) {
  auto *dev = (struct serial8250_device *)_dp;


	uint16_t offset;
	bool ret = true;

	dev->mutex.lock();

	offset = port - dev->iobase;

	switch (offset) {
	case UART_RX:
		if (dev->lcr & UART_LCR_DLAB)
			ioport_write<char>(data, dev->dll);
		else
			serial8250_rx(dev, data);
		break;
	case UART_IER:
		if (dev->lcr & UART_LCR_DLAB)
			ioport_write<char>(data, dev->dlm);
		else
			ioport_write<char>(data, dev->ier);
		break;
	case UART_IIR:
		ioport_write<char>(data, dev->iir | UART_IIR_TYPE_BITS);
		break;
	case UART_LCR:
		ioport_write<char>(data, dev->lcr);
		break;
	case UART_MCR:
		ioport_write<char>(data, dev->mcr);
		break;
	case UART_LSR:
		ioport_write<char>(data, dev->lsr);
		break;
	case UART_MSR:
		ioport_write<char>(data, dev->msr);
		break;
	case UART_SCR:
		ioport_write<char>(data, dev->scr);
		break;
	default:
		ret = false;
		break;
	}

	// serial8250_update_irq(vcpu->kvm, dev);

  dev->mutex.unlock();

	return ret;
}

static int serial_out(mobo::port_t port, void *data, int sz, void *_dp) {
  auto *dev = (struct serial8250_device *)_dp;

  uint16_t offset;
  bool ret = true;
  auto *addr = (char *)data;
  dev->mutex.lock();
  offset = port - dev->iobase;

  switch (offset) {
    case UART_TX:
      if (dev->lcr & UART_LCR_DLAB) {
        dev->dll = ioport_read<char>(data);
        break;
      }

      /* Loopback mode */
      if (dev->mcr & UART_MCR_LOOP) {
        if (dev->rxcnt < FIFO_LEN) {
          dev->rxbuf[dev->rxcnt++] = *addr;
          dev->lsr |= UART_LSR_DR;
        }
        break;
      }

      if (dev->txcnt < FIFO_LEN) {
        dev->txbuf[dev->txcnt++] = *addr;
        dev->lsr &= ~UART_LSR_TEMT;
        if (dev->txcnt == FIFO_LEN / 2) dev->lsr &= ~UART_LSR_THRE;
        serial8250_flush_tx(dev);
      } else {
        /* Should never happpen */
        dev->lsr &= ~(UART_LSR_TEMT | UART_LSR_THRE);
      }
      break;
    case UART_IER:
      if (!(dev->lcr & UART_LCR_DLAB))
        dev->ier = ioport_read<char>(data) & 0x0f;
      else
        dev->dlm = ioport_read<char>(data);
      break;
    case UART_FCR:
      dev->fcr = ioport_read<char>(data);
      break;
    case UART_LCR:
      dev->lcr = ioport_read<char>(data);
      break;
    case UART_MCR:
      dev->mcr = ioport_read<char>(data);
      break;
    case UART_LSR:
      /* Factory test */
      break;
    case UART_MSR:
      /* Not used */
      break;
    case UART_SCR:
      dev->scr = ioport_read<char>(data);
      break;
    default:
      ret = false;
      break;
  }

  // serial8250_update_irq(vcpu->kvm, dev);

  dev->mutex.unlock();
  return ret;
}

static int serial_init(mobo::device_manager *dm) {
  int i;
  for (i = 0; i < DEVICE_COUNT; i++) {
    struct serial8250_device *dev = &devices[i];
    for (int i = 0; i < 8; i++)
      dm->hook_io(dev->iobase + i, serial_in, serial_out, dev);
  }
  return 0;
}

mobo_device_register("serial8250", serial_init);
