#include <dev/driver.h>
#include <module.h>

#define BIT(N) (1UL << (N))

#define SIFIVE_SPI_DRIVER_NAME "sifive-spi"

#define LOG(...) PFXLOG(BLU "sifive-spi", __VA_ARGS__)

#define SIFIVE_SPI_MAX_CS 32
#define SIFIVE_SPI_DEFAULT_DEPTH 8
#define SIFIVE_SPI_DEFAULT_MAX_BITS 8

/* register offsets */
#define SIFIVE_SPI_REG_SCKDIV 0x00  /* Serial clock divisor */
#define SIFIVE_SPI_REG_SCKMODE 0x04 /* Serial clock mode */
#define SIFIVE_SPI_REG_CSID 0x10    /* Chip select ID */
#define SIFIVE_SPI_REG_CSDEF 0x14   /* Chip select default */
#define SIFIVE_SPI_REG_CSMODE 0x18  /* Chip select mode */
#define SIFIVE_SPI_REG_DELAY0 0x28  /* Delay control 0 */
#define SIFIVE_SPI_REG_DELAY1 0x2c  /* Delay control 1 */
#define SIFIVE_SPI_REG_FMT 0x40     /* Frame format */
#define SIFIVE_SPI_REG_TXDATA 0x48  /* Tx FIFO data */
#define SIFIVE_SPI_REG_RXDATA 0x4c  /* Rx FIFO data */
#define SIFIVE_SPI_REG_TXMARK 0x50  /* Tx FIFO watermark */
#define SIFIVE_SPI_REG_RXMARK 0x54  /* Rx FIFO watermark */
#define SIFIVE_SPI_REG_FCTRL 0x60   /* SPI flash interface control */
#define SIFIVE_SPI_REG_FFMT 0x64    /* SPI flash instruction format */
#define SIFIVE_SPI_REG_IE 0x70      /* Interrupt Enable Register */
#define SIFIVE_SPI_REG_IP 0x74      /* Interrupt Pendings Register */

/* sckdiv bits */
#define SIFIVE_SPI_SCKDIV_DIV_MASK 0xfffU

/* sckmode bits */
#define SIFIVE_SPI_SCKMODE_PHA BIT(0)
#define SIFIVE_SPI_SCKMODE_POL BIT(1)
#define SIFIVE_SPI_SCKMODE_MODE_MASK (SIFIVE_SPI_SCKMODE_PHA | SIFIVE_SPI_SCKMODE_POL)

/* csmode bits */
#define SIFIVE_SPI_CSMODE_MODE_AUTO 0U
#define SIFIVE_SPI_CSMODE_MODE_HOLD 2U
#define SIFIVE_SPI_CSMODE_MODE_OFF 3U

/* delay0 bits */
#define SIFIVE_SPI_DELAY0_CSSCK(x) ((u32)(x))
#define SIFIVE_SPI_DELAY0_CSSCK_MASK 0xffU
#define SIFIVE_SPI_DELAY0_SCKCS(x) ((u32)(x) << 16)
#define SIFIVE_SPI_DELAY0_SCKCS_MASK (0xffU << 16)

/* delay1 bits */
#define SIFIVE_SPI_DELAY1_INTERCS(x) ((u32)(x))
#define SIFIVE_SPI_DELAY1_INTERCS_MASK 0xffU
#define SIFIVE_SPI_DELAY1_INTERXFR(x) ((u32)(x) << 16)
#define SIFIVE_SPI_DELAY1_INTERXFR_MASK (0xffU << 16)

/* fmt bits */
#define SIFIVE_SPI_FMT_PROTO_SINGLE 0U
#define SIFIVE_SPI_FMT_PROTO_DUAL 1U
#define SIFIVE_SPI_FMT_PROTO_QUAD 2U
#define SIFIVE_SPI_FMT_PROTO_MASK 3U
#define SIFIVE_SPI_FMT_ENDIAN BIT(2)
#define SIFIVE_SPI_FMT_DIR BIT(3)
#define SIFIVE_SPI_FMT_LEN(x) ((u32)(x) << 16)
#define SIFIVE_SPI_FMT_LEN_MASK (0xfU << 16)

/* txdata bits */
#define SIFIVE_SPI_TXDATA_DATA_MASK 0xffU
#define SIFIVE_SPI_TXDATA_FULL BIT(31)

/* rxdata bits */
#define SIFIVE_SPI_RXDATA_DATA_MASK 0xffU
#define SIFIVE_SPI_RXDATA_EMPTY BIT(31)

/* ie and ip bits */
#define SIFIVE_SPI_IP_TXWM BIT(0)
#define SIFIVE_SPI_IP_RXWM BIT(1)

static dev::ProbeResult probe(ck::ref<hw::Device> dev) {
  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->is_compat("sifive,spi0")) {
      LOG("Found device @%08llx. irq=%d\n", mmio->address(), mmio->interrupt);
      return dev::ProbeResult::Attach;
    }
  }
  return dev::ProbeResult::Ignore;
};


namespace sifive {
  class SpiController : public dev::CharDevice {
   public:
    using dev::CharDevice::CharDevice;
    ~SpiController() {}

    void init(void) override;

    inline void spi_write(int offset, uint32_t value) { regs[offset] = value; }
    inline uint32_t spi_read(int offset) { return regs[offset]; }

    void wait(uint32_t bit, bool poll = true);

    void spi_tx(const uint8_t *tx_ptr);
    void spi_rx(uint8_t *rx_ptr);

   private:
		uint32_t fifo_depth = SIFIVE_SPI_DEFAULT_DEPTH;
		uint32_t max_bits_per_word = 0;
    volatile uint32_t *regs = NULL;
  };

}  // namespace sifive
void sifive::SpiController::init(void) {
  auto mmio = dev()->cast<hw::MMIODevice>();

  // the registers are simply at the base
  regs = (uint32_t *)p2v(mmio->address());

  /* Watermark interrupts are disabled by default */
  spi_write(SIFIVE_SPI_REG_IE, 0);


  /* Default watermark FIFO threshold values */
  spi_write(SIFIVE_SPI_REG_TXMARK, 1);
  spi_write(SIFIVE_SPI_REG_RXMARK, 0);

  /* Set CS/SCK Delays and Inactive Time to defaults */
  spi_write(SIFIVE_SPI_REG_DELAY0, SIFIVE_SPI_DELAY0_CSSCK(1) | SIFIVE_SPI_DELAY0_SCKCS(1));
  spi_write(SIFIVE_SPI_REG_DELAY1, SIFIVE_SPI_DELAY1_INTERCS(1) | SIFIVE_SPI_DELAY1_INTERXFR(0));

  /* Exit specialized memory-mapped SPI flash mode */
  spi_write(SIFIVE_SPI_REG_FCTRL, 0);

	fifo_depth = mmio->get_prop_int("sifive,fifo-depth").or_default(SIFIVE_SPI_DEFAULT_DEPTH);
	max_bits_per_word = mmio->get_prop_int("sifive,max-bits-per-word").or_default(SIFIVE_SPI_DEFAULT_MAX_BITS);
	if (max_bits_per_word < 8) {
		panic("Only 8bit SPI words supported by the driver\n");
	}


  printf("================================================\n");

}

void sifive::SpiController::wait(uint32_t bit, bool poll) {
  if (poll) {
    u32 cr;
    do {
      cr = spi_read(SIFIVE_SPI_REG_IP);
    } while (!(cr & bit));
  } else {
    panic("don't know what to do...\n");
    /*
    reinit_completion(&spi->done);
    sifive_spi_write(spi, SIFIVE_SPI_REG_IE, bit);
    wait_for_completion(&spi->done);
    */
  }
}

void sifive::SpiController::spi_tx(const uint8_t *tx_ptr) {
  if ((spi_read(SIFIVE_SPI_REG_TXDATA) & SIFIVE_SPI_TXDATA_FULL) != 0) {
		printf(KERN_WARN "sifive-spi: TXDATA_FULL\n");
	}
  spi_write(SIFIVE_SPI_REG_TXDATA, *tx_ptr & SIFIVE_SPI_TXDATA_DATA_MASK);
}

void sifive::SpiController::spi_rx(uint8_t *rx_ptr) {
	u32 data = spi_read(SIFIVE_SPI_REG_RXDATA);

	if ((data & SIFIVE_SPI_RXDATA_EMPTY) != 0) {
		printf(KERN_WARN "sifive-spi: RXDATA_EMPTY\n");
	}
	*rx_ptr = data & SIFIVE_SPI_RXDATA_DATA_MASK;
}

driver_init(SIFIVE_SPI_DRIVER_NAME, sifive::SpiController, probe);
