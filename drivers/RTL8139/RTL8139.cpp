#include "RTL8139.h"

#include <asm.h>
#include <arch.h>
#include <mem.h>
#include <phys.h>
#include <util.h>

#define RTLDEBUG

#ifdef RTLDEBUG
#define INFO(fmt, args...) KWARN("RTL8139: " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

// TODO: make this not global
static rtl8139 *main_dev = NULL;

static void rtl_irq_handler(int i, reg_t *tf, void *) {
  printk("here\n");
  if (main_dev != NULL) {
    main_dev->handle_irq();
  }
}

rtl8139::rtl8139(pci::device &dev, u8 irq) : m_dev(dev) {
  m_dev.enable_bus_mastering();

  m_io_base = m_dev.get_bar(0).raw & ~1;
  m_interrupt_line = m_dev.read<u8>(0x3C /* magic constant :: PCI_INTERRUPT_LINE */);

  INFO("IO port base: %x\n", m_io_base);
  INFO("Interrupt line: %u\n", m_interrupt_line);

  // we add space to account for overhang from the last packet - the rtl8139
  // can optionally guarantee that packets will be contiguous by
  // purposefully overrunning the rx buffer
  auto rx_pgcount = round_up(RX_BUFFER_SIZE + PACKET_SIZE_MAX, PGSIZE) / PGSIZE;
  INFO("rx_pgcount = %d\n", rx_pgcount);

  m_rx_buffer_addr = (u64)phys::alloc(rx_pgcount);
  INFO("RX buffer: P%p\n", m_rx_buffer_addr);

  INFO("%d\n", TX_BUFFER_SIZE * RTL8139_TX_BUFFER_COUNT);

  auto tx_pgcount = round_up(TX_BUFFER_SIZE * RTL8139_TX_BUFFER_COUNT, PGSIZE) / PGSIZE;
  INFO("tx_pgcount = %d\n", tx_pgcount);
  auto tx_buffer_addr = (u64)phys::alloc(tx_pgcount);
  for (int i = 0; i < RTL8139_TX_BUFFER_COUNT; i++) {
    m_tx_buffer_addr[i] = tx_buffer_addr + TX_BUFFER_SIZE * i;
    INFO("TX buffer %d: P%p\n", i, m_tx_buffer_addr[i]);
  }

  m_packet_buffer = malloc(PACKET_SIZE_MAX);

  reset();

  read_mac_address();

  main_dev = this;
  INFO("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4],
       mac[5]);


  irq::install(m_interrupt_line, rtl_irq_handler, "RTL8139 Ethernet Card");
}

rtl8139::~rtl8139() {
  free(m_packet_buffer);
}

void rtl8139::out8(u16 a, u8 d) {
  outb(m_io_base + a, d);
}
void rtl8139::out16(u16 a, u16 d) {
  outw(m_io_base + a, d);
}
void rtl8139::out32(u16 a, u32 d) {
  outl(m_io_base + a, d);
}
u8 rtl8139::in8(u16 address) {
  return inb(m_io_base + address);
}
u16 rtl8139::in16(u16 address) {
  return inw(m_io_base + address);
}
u32 rtl8139::in32(u16 address) {
  return inl(m_io_base + address);
}

void rtl8139::reset(void) {
  m_rx_buffer_offset = 0;
  m_tx_next_buffer = 0;

  out8(REG_COMMAND, COMMAND_RESET);
  while ((in8(REG_COMMAND) & COMMAND_RESET) != 0)
    ;

  // unlock config registers
  out8(REG_CFG9346, CFG9346_EEM0 | CFG9346_EEM1);
  // turn on multicast
  out32(REG_MAR0, 0xffffffff);
  out32(REG_MAR4, 0xffffffff);
  // enable rx/tx
  out8(REG_COMMAND, COMMAND_RX_ENABLE | COMMAND_TX_ENABLE);
  // device might be in sleep mode, this will take it out
  out8(REG_CONFIG1, 0);
  // set up rx buffer
  out32(REG_RXBUF, m_rx_buffer_addr);
  // reset missed packet counter
  out8(REG_MPC, 0);
  // "basic mode control register" options - 100mbit, full duplex, auto
  // negotiation
  out16(REG_BMCR, BMCR_SPEED | BMCR_AUTO_NEGOTIATE | BMCR_DUPLEX);
  // enable flow control
  out8(REG_MSR, MSR_RX_FLOW_CONTROL_ENABLE);
  // configure rx: accept physical (MAC) match, multicast, and broadcast,
  // use the optional contiguous packet feature, the maximum dma transfer
  // size, a 32k buffer, and no fifo threshold
  out32(REG_RXCFG, RXCFG_APM | RXCFG_AM | RXCFG_AB | RXCFG_WRAP_INHIBIT | RXCFG_MAX_DMA_UNLIMITED |
                       RXCFG_RBLN_32K | RXCFG_FTH_NONE);
  // configure tx: default retry count (16), max DMA burst size of 1024
  // bytes, interframe gap time of the only allowable value. the DMA burst
  // size is important - silent failures have been observed with 2048 bytes.
  out32(REG_TXCFG, TXCFG_TXRR_ZERO | TXCFG_MAX_DMA_1K | TXCFG_IFG11);
  // tell the chip where we want it to DMA from for outgoing packets.
  for (int i = 0; i < 4; i++)
    out32(REG_TXADDR0 + (i * 4), m_tx_buffer_addr[i]);
  // re-lock config registers
  out8(REG_CFG9346, CFG9346_NONE);
  // enable rx/tx again in case they got turned off (apparently some cards
  // do this?)
  out8(REG_COMMAND, COMMAND_RX_ENABLE | COMMAND_TX_ENABLE);

  // choose irqs, then clear any pending
  out16(REG_IMR, INT_RXOK | INT_RXERR | INT_TXOK | INT_TXERR | INT_RX_BUFFER_OVERFLOW |
                     INT_LINK_CHANGE | INT_RX_FIFO_OVERFLOW | INT_LENGTH_CHANGE | INT_SYSTEM_ERROR);
  out16(REG_ISR, 0xffff);
}

void rtl8139::handle_irq() {
  for (;;) {
    int status = in16(REG_ISR);
    out16(REG_ISR, status);

    INFO(".handle_irq status=%#04x\n", status);

    if ((status &
         (INT_RXOK | INT_RXERR | INT_TXOK | INT_TXERR | INT_RX_BUFFER_OVERFLOW | INT_LINK_CHANGE |
          INT_RX_FIFO_OVERFLOW | INT_LENGTH_CHANGE | INT_SYSTEM_ERROR)) == 0)
      break;

    if (status & INT_RXOK) {
      KWARN("RTL8139NetworkAdapter: rx ready\n");
      receive();
    }
    if (status & INT_RXERR) {
      KERR("RTL8139NetworkAdapter: rx error - resetting device\n");
      reset();
    }
    if (status & INT_TXOK) {
      KWARN("RTL8139NetworkAdapter: tx complete\n");
    }
    if (status & INT_TXERR) {
      KERR("RTL8139NetworkAdapter: tx error - resetting device\n");
      reset();
    }
    if (status & INT_RX_BUFFER_OVERFLOW) {
      KERR("RTL8139NetworkAdapter: rx buffer overflow\n");
    }
    if (status & INT_LINK_CHANGE) {
      auto m_link_up = (in8(REG_MSR) & MSR_LINKB) == 0;
      KWARN("RTL8139NetworkAdapter: link status changed up=%d\n", m_link_up);
    }
    if (status & INT_RX_FIFO_OVERFLOW) {
      KERR("RTL8139NetworkAdapter: rx fifo overflow\n");
    }
    if (status & INT_LENGTH_CHANGE) {
      KWARN("RTL8139NetworkAdapter: cable length change\n");
    }
    if (status & INT_SYSTEM_ERROR) {
      KERR("RTL8139NetworkAdapter: system error - resetting device\n");
      reset();
    }
  }
}

void rtl8139::read_mac_address(void) {
  for (int i = 0; i < 6; i++)
    mac[i] = in8(REG_MAC + i);
}

void rtl8139::receive(void) {
  auto *start_of_packet = (const u8 *)(m_rx_buffer_addr + m_rx_buffer_offset);

  u16 status = *(const u16 *)(start_of_packet + 0);
  u16 length = *(const u16 *)(start_of_packet + 2);

  INFO(".receive status=%04x length=%d offset=%d\n", status, length, m_rx_buffer_offset);
  if (!(status & RX_OK) ||
      (status & (RX_INVALID_SYMBOL_ERROR | RX_CRC_ERROR | RX_FRAME_ALIGNMENT_ERROR)) ||
      (length >= PACKET_SIZE_MAX) || (length < PACKET_SIZE_MIN)) {
    KERR("rtl8139::receive got bad packet status=%04x length=%d\n", status, length);
    reset();
    return;
  }

  // we never have to worry about the packet wrapping around the buffer,
  // since we set RXCFG_WRAP_INHIBIT, which allows the rtl8139 to write data
  // past the end of the alloted space.
  memcpy((u8 *)m_packet_buffer, (const u8 *)(start_of_packet + 4), length - 4);
  // let the card know that we've read this data
  m_rx_buffer_offset = ((m_rx_buffer_offset + length + 4 + 3) & ~3) % RX_BUFFER_SIZE;
  out16(REG_CAPR, m_rx_buffer_offset - 0x10);
  m_rx_buffer_offset %= RX_BUFFER_SIZE;

  hexdump(m_packet_buffer, length - 4);
}

void rtl8139::send_raw(const void *data, int length) {
  INFO(".send_raw length=%d\n", length);
  if (length > PACKET_SIZE_MAX) {
    KWARN("rtl8139: packet was too big; discarding\n");
    return;
  }

  int hw_buffer = -1;
  for (int i = 0; i < RTL8139_TX_BUFFER_COUNT; i++) {
    int potential_buffer = (m_tx_next_buffer + i) % 4;

    auto status = in32(REG_TXSTATUS0 + (potential_buffer * 4));
    if (status & TX_STATUS_OWN) {
      hw_buffer = potential_buffer;
      break;
    }
  }

  if (hw_buffer == -1) {
    KERR("rtl8139: hardware buffers full; discarding packet\n");
    return;
  } else {
    INFO("chose buffer %d @ %p\n", hw_buffer, m_tx_buffer_addr[hw_buffer]);
    m_tx_next_buffer = (hw_buffer + 1) % 4;
  }

  memcpy((void *)(p2v(m_tx_buffer_addr[hw_buffer])), data, length);
  memset((void *)((u64)p2v(m_tx_buffer_addr[hw_buffer]) + length), 0, TX_BUFFER_SIZE - length);

  // the rtl8139 will not actually emit packets onto the network if they're
  // smaller than 64 bytes. the rtl8139 adds a checksum to the end of each
  // packet, and that checksum is four bytes long, so we pad the packet to
  // 60 bytes if necessary to make sure the whole thing is large enough.
  if (length < 60) {
    KWARN("rtl8139: adjusting payload size from %d to 60\n", length);
    length = 60;
  }

  out32(REG_TXSTATUS0 + (hw_buffer * 4), length);
}

unique_ptr<rtl8139> d;
void RTL8139_init(void) {
  pci::device *edev = nullptr;
  pci::walk_devices([&](pci::device *dev) {
    // check if the device is an e1000 device
    if (dev->is_device(0x10EC, 0x8139)) {
      edev = dev;
    }
  });

  if (edev != nullptr) {
    KINFO("found!\n");

    d = make_unique<rtl8139>(*edev, 0);

    char buf[512];

    buf[0] = 0x52;
    buf[1] = 0x54;
    buf[2] = 0x00;
    buf[3] = 0x12;
    buf[4] = 0x34;
    buf[5] = 0x57;

    buf[0 + 6] = 0x52;
    buf[1 + 6] = 0x54;
    buf[2 + 6] = 0x00;
    buf[3 + 6] = 0x12;
    buf[4 + 6] = 0x34;
    buf[5 + 6] = 0x57;

    buf[12] = 0x08;
    buf[13] = 0x06;

    d->send_raw(buf, 512);
    while (1) {
    }
  }
}

// module_init("RTL8140", RTL8139_init);
