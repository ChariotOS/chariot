#include <idt.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <phys.h>
#include <printk.h>
#include <util.h>

#define E1000_DEBUG

#ifdef E1000_DEBUG
#define INFO(fmt, args...) printk("[E1000] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

#define REG_CTRL 0x0000
#define REG_STATUS 0x0008
#define REG_EEPROM 0x0014
#define REG_CTRL_EXT 0x0018
#define REG_IMASK 0x00D0
#define REG_RCTRL 0x0100
#define REG_RXDESCLO 0x2800
#define REG_RXDESCHI 0x2804
#define REG_RXDESCLEN 0x2808
#define REG_RXDESCHEAD 0x2810
#define REG_RXDESCTAIL 0x2818
#define REG_TCTRL 0x0400
#define REG_TXDESCLO 0x3800
#define REG_TXDESCHI 0x3804
#define REG_TXDESCLEN 0x3808
#define REG_TXDESCHEAD 0x3810
#define REG_TXDESCTAIL 0x3818
#define REG_RDTR 0x2820              // RX Delay Timer Register
#define REG_RXDCTL 0x3828            // RX Descriptor Control
#define REG_RADV 0x282C              // RX Int. Absolute Delay Timer
#define REG_RSRPD 0x2C00             // RX Small Packet Detect Interrupt
#define REG_TIPG 0x0410              // Transmit Inter Packet Gap
#define ECTRL_SLU 0x40               // set link up
#define RCTL_EN (1 << 1)             // Receiver Enable
#define RCTL_SBP (1 << 2)            // Store Bad Packets
#define RCTL_UPE (1 << 3)            // Unicast Promiscuous Enabled
#define RCTL_MPE (1 << 4)            // Multicast Promiscuous Enabled
#define RCTL_LPE (1 << 5)            // Long Packet Reception Enable
#define RCTL_LBM_NONE (0 << 6)       // No Loopback
#define RCTL_LBM_PHY (3 << 6)        // PHY or external SerDesc loopback
#define RTCL_RDMTS_HALF (0 << 8)     // Free Buffer Threshold is 1/2 of RDLEN
#define RTCL_RDMTS_QUARTER (1 << 8)  // Free Buffer Threshold is 1/4 of RDLEN
#define RTCL_RDMTS_EIGHTH (2 << 8)   // Free Buffer Threshold is 1/8 of RDLEN
#define RCTL_MO_36 (0 << 12)         // Multicast Offset - bits 47:36
#define RCTL_MO_35 (1 << 12)         // Multicast Offset - bits 46:35
#define RCTL_MO_34 (2 << 12)         // Multicast Offset - bits 45:34
#define RCTL_MO_32 (3 << 12)         // Multicast Offset - bits 43:32
#define RCTL_BAM (1 << 15)           // Broadcast Accept Mode
#define RCTL_VFE (1 << 18)           // VLAN Filter Enable
#define RCTL_CFIEN (1 << 19)         // Canonical Form Indicator Enable
#define RCTL_CFI (1 << 20)           // Canonical Form Indicator Bit Value
#define RCTL_DPF (1 << 22)           // Discard Pause Frames
#define RCTL_PMCF (1 << 23)          // Pass MAC Control Frames
#define RCTL_SECRC (1 << 26)         // Strip Ethernet CRC

// Buffer Sizes
#define RCTL_BSIZE_256 (3 << 16)
#define RCTL_BSIZE_512 (2 << 16)
#define RCTL_BSIZE_1024 (1 << 16)
#define RCTL_BSIZE_2048 (0 << 16)
#define RCTL_BSIZE_4096 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384 ((1 << 16) | (1 << 25))

// Transmit Command

#define CMD_EOP (1 << 0)   // End of Packet
#define CMD_IFCS (1 << 1)  // Insert FCS
#define CMD_IC (1 << 2)    // Insert Checksum
#define CMD_RS (1 << 3)    // Report Status
#define CMD_RPS (1 << 4)   // Report Packet Sent
#define CMD_VLE (1 << 6)   // VLAN Packet Enable
#define CMD_IDE (1 << 7)   // Interrupt Delay Enable

// TCTL Register

#define TCTL_EN (1 << 1)       // Transmit Enable
#define TCTL_PSP (1 << 3)      // Pad Short Packets
#define TCTL_CT_SHIFT 4        // Collision Threshold
#define TCTL_COLD_SHIFT 12     // Collision Distance
#define TCTL_SWXOFF (1 << 22)  // Software XOFF Transmission
#define TCTL_RTLC (1 << 24)    // Re-transmit on Late Collision

#define TSTA_DD (1 << 0)  // Descriptor Done
#define TSTA_EC (1 << 1)  // Excess Collisions
#define TSTA_LC (1 << 2)  // Late Collision
#define LSTA_TU (1 << 3)  // Transmit Underrun

// STATUS Register

#define STATUS_FD 0x01
#define STATUS_LU 0x02
#define STATUS_TXOFF 0x08
#define STATUS_SPEED 0xC0
#define STATUS_SPEED_10MB 0x00
#define STATUS_SPEED_100MB 0x40
#define STATUS_SPEED_1000MB1 0x80
#define STATUS_SPEED_1000MB2 0xC0

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

struct [[gnu::packed]] e1000_rx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint16_t checksum;
  volatile uint8_t status;
  volatile uint8_t errors;
  volatile uint16_t special;
};

struct [[gnu::packed]] e1000_tx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint8_t cso;
  volatile uint8_t cmd;
  volatile uint8_t status;
  volatile uint8_t css;
  volatile uint16_t special;
};

class e1000 : public refcounted<e1000> {
 private:
  pci::device *dev;
  u8 bar_type;          // type of bar 0
  u16 io_base;          // the base address for PIO
  u64 mem_base;         // mmio base address
  bool eerprom_exists;  // a flag indicating if eeprom exists
  u8 mac[6];            // a buffer to store the mac address for the e1000 card

  e1000_rx_desc *rx_descs[E1000_NUM_RX_DESC];  // receive descriptor buffers
  e1000_tx_desc *tx_descs[E1000_NUM_TX_DESC];  // receive descriptor buffers

  u16 rx_cur;
  u16 tx_cur;

  // send commands and read results from NICs using either MMIO or IO ports
  void write_cmd(u16 p_addr, u32 p_value);
  u32 read_cmd(u16 p_addr);

  bool detect_eeprom(
      void);  // return true if eeprom exists, setting the eeprom exists bool
  u32 eeprom_read(u8 addr);  // read 4 bytes from the eeprom
  bool read_mac_addr(void);
  void start_link();        // setup the network
  void rxinit();            // initialize rx descriptors and buffers;
  void txinit();            // initialize tx descriptors and buffers;
  void enable_interrupt();  // ...
  void handle_receive();    // handle a packet reception

 public:
  e1000(pci::device *pci_dev);
  ~e1000();

  // called by the interrupt handler
  void fire(trapframe *fr);

  u8 *get_mac_address(void);

  int send_packet(const void *data, u16 len);

  bool start(void);
};

void e1000::write_cmd(u16 addr, u32 val) {
  if (bar_type == 0) {
    reinterpret_cast<u32 *>((u64)p2v(mem_base) + addr)[0] = val;
  } else {
    outl(io_base, addr);
    outl(io_base + 4, val);
  }
}

u32 e1000::read_cmd(u16 addr) {
  if (bar_type == 0) {
    return reinterpret_cast<u32 *>((u64)p2v(mem_base) + addr)[0];
  } else {
    outl(io_base, addr);
    return inl(io_base + 4);
  }
}

bool e1000::detect_eeprom(void) {
  u32 val = 0;
  write_cmd(REG_EEPROM, 0x1);
  for (int i = 0; i < 1000 && !eerprom_exists; i++) {
    val = read_cmd(REG_EEPROM);
    if (val & 0x10)
      eerprom_exists = true;
    else
      eerprom_exists = false;
  }
  return eerprom_exists;
}

u32 e1000::eeprom_read(u8 addr) {
  u16 data = 0;
  u32 tmp = 0;
  if (eerprom_exists) {
    write_cmd(REG_EEPROM, (1) | ((u32)(addr) << 8));
    while (!((tmp = read_cmd(REG_EEPROM)) & (1 << 4))) {
    };
  } else {
    write_cmd(REG_EEPROM, (1) | ((u32)(addr) << 2));
    while (!((tmp = read_cmd(REG_EEPROM)) & (1 << 1)))
      ;
  }
  data = (u16)((tmp >> 16) & 0xFFFF);
  return data;
}

bool e1000::read_mac_addr() {
  if (eerprom_exists) {
    u32 temp;
    temp = eeprom_read(0);
    mac[0] = temp & 0xff;
    mac[1] = temp >> 8;
    temp = eeprom_read(1);
    mac[2] = temp & 0xff;
    mac[3] = temp >> 8;
    temp = eeprom_read(2);
    mac[4] = temp & 0xff;
    mac[5] = temp >> 8;
  } else {
    u8 *mem_base_mac_8 = (u8 *)(mem_base + 0x5400);
    u32 *mem_base_mac_32 = (u32 *)(mem_base + 0x5400);

    printk("%p\n", mem_base_mac_32);
    if (mem_base_mac_32[0] != 0) {
      for (int i = 0; i < 6; i++) {
        mac[i] = mem_base_mac_8[i];
      }
    } else
      return false;
  }
  return true;
}

void e1000::rxinit() {
  uint8_t *ptr;
  struct e1000_rx_desc *descs;

  // Allocate buffer for receive descriptors. For simplicity, in my case
  // khmalloc returns a virtual address that is identical to it physical mapped
  // address. In your case you should handle virtual and physical addresses as
  // the addresses passed to the NIC should be physical ones

  // auto size = sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC + 16;

  ptr = (u8 *)phys::alloc();
  // ptr = (uint8_t *)(kmalloc(size));

  descs = (struct e1000_rx_desc *)p2v(ptr);
  for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
    rx_descs[i] = (struct e1000_rx_desc *)((uint8_t *)descs + i * 16);
    rx_descs[i]->addr = (uint64_t)(uint8_t *)(kmalloc(8192 + 16));
    rx_descs[i]->status = 0;
  }

  INFO("RX virt: %16zx\n", descs);
  INFO("RX phys: %16zx\n", ptr);

  write_cmd(REG_TXDESCLO, (uint32_t)((uint64_t)ptr >> 32));
  write_cmd(REG_TXDESCHI, (uint32_t)((uint64_t)ptr & 0xFFFFFFFF));

  write_cmd(REG_RXDESCLO, (uint64_t)ptr);
  write_cmd(REG_RXDESCHI, 0);

  write_cmd(REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);

  write_cmd(REG_RXDESCHEAD, 0);
  write_cmd(REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
  rx_cur = 0;
  write_cmd(REG_RCTRL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE |
                           RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BAM |
                           RCTL_SECRC | RCTL_BSIZE_8192);
}

void e1000::txinit() {
  u8 *ptr;
  struct e1000_tx_desc *descs;
  // Allocate buffer for receive descriptors. For simplicity, in my case
  // khmalloc returns a virtual address that is identical to it physical mapped
  // address. In your case you should handle virtual and physical addresses as
  // the addresses passed to the NIC should be physical ones

  // auto sz = sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC + 16;
  // ptr = (uint8_t *)(kmalloc(sz));

  ptr = (u8 *)phys::alloc();

  descs = (struct e1000_tx_desc *)p2v(ptr);
  for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
    tx_descs[i] = (struct e1000_tx_desc *)((uint8_t *)descs + i * 16);
    tx_descs[i]->addr = 0;
    tx_descs[i]->cmd = 0;
    tx_descs[i]->status = TSTA_DD;
  }
  INFO("TX virt: %16zx\n", descs);
  INFO("TX phys: %16zx\n", ptr);

  write_cmd(REG_TXDESCHI, (uint32_t)((uint64_t)ptr >> 32));
  write_cmd(REG_TXDESCLO, (uint32_t)((uint64_t)ptr & 0xFFFFFFFF));

  // now setup total length of descriptors
  write_cmd(REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);

  // setup numbers
  write_cmd(REG_TXDESCHEAD, 0);
  write_cmd(REG_TXDESCTAIL, 0);
  tx_cur = 0;
  write_cmd(REG_TCTRL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) |
                           (64 << TCTL_COLD_SHIFT) | TCTL_RTLC);

  // This line of code overrides the one before it but I left both to highlight
  // that the previous one works with e1000 cards, but for the e1000e cards you
  // should set the TCTRL register as follows. For detailed description of each
  // bit, please refer to the Intel Manual. In the case of I217 and 82577LM
  // packets will not be sent if the TCTRL is not configured using the following
  // bits.
  write_cmd(REG_TCTRL, 0b0110000000000111111000011111010);
  write_cmd(REG_TIPG, 0x0060200A);
}

void e1000::enable_interrupt() {
  write_cmd(REG_IMASK, 0x1F6DC);
  write_cmd(REG_IMASK, 0xff & ~4);
  read_cmd(0xc0);
}

static void e1000_interrupt(int intr, trapframe *fr);

e1000::e1000(pci::device *dev)
    : dev(dev) /* : NetworkDriver(p_pciConfigHeader) */ {
  auto bar0 = dev->get_bar(0);
  bar_type = (u8)bar0.type;
  // Get BAR0 type, io_base address and MMIO base address
  io_base = dev->get_bar(PCI_BAR_IO).raw & ~1;
  mem_base = dev->get_bar(0).raw & ~3;

  // Off course you will need here to map the memory address into you page
  // tables and use corresponding virtual addresses

  // Enable bus mastering
  dev->enable_bus_mastering();
}

bool e1000::start(void) {
  eerprom_exists = false;
  detect_eeprom();
  if (!read_mac_addr()) return false;
  INFO("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1],
         mac[2], mac[3], mac[4], mac[5]);
  start_link();

  for (int i = 0; i < 0x80; i++) write_cmd(0x5200 + i * 4, 0);

  interrupt_register(32 + dev->interrupt, e1000_interrupt);
  enable_interrupt();
  rxinit();
  txinit();
  INFO("card started\n");
  return true;
}

e1000::~e1000(void) {}

void e1000::fire(trapframe *p_interruptContext) {
  /* This might be needed here if your handler doesn't clear interrupts from
     each device and must be done before EOI if using the PIC.
     Without this, the card will spam interrupts as the int-line will stay high.
   */
  write_cmd(REG_IMASK, 0x1);


  uint32_t status = read_cmd(0xc0);

  INFO("IRQ. STATUS=%08x\n", status);
  if (status & 0x04) {
    start_link();
  } else if (status & 0x10) {
    // good threshold
  } else if (status & 0x80) {
    handle_receive();
  }
}

void e1000::start_link(void) {
  // return (in32(REG_STATUS) & STATUS_LU);
}

void e1000::handle_receive() {
  uint16_t old_cur;
  bool got_packet = false;

  while ((rx_descs[rx_cur]->status & 0x1)) {
    got_packet = true;
    uint8_t *buf = (uint8_t *)rx_descs[rx_cur]->addr;
    uint16_t len = rx_descs[rx_cur]->length;

    INFO("RX: %d bytes at %p\n", len, buf);

    hexdump(buf, len);

    // Here you should inject the received packet into your network stack

    rx_descs[rx_cur]->status = 0;
    old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    write_cmd(REG_RXDESCTAIL, old_cur);
  }
}

int e1000::send_packet(const void *p_data, uint16_t p_len) {
  tx_descs[tx_cur]->addr = (uint64_t)p_data;
  tx_descs[tx_cur]->length = p_len;
  tx_descs[tx_cur]->cmd = CMD_EOP | CMD_IFCS | CMD_RS;
  tx_descs[tx_cur]->status = 0;
  uint8_t old_cur = tx_cur;
  tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
  write_cmd(REG_TXDESCTAIL, tx_cur);
  while (!(tx_descs[old_cur]->status & 0xff))
    ;
  return 0;
}

static ref<e1000> e1000_inst;
static void e1000_interrupt(int intr, trapframe *fr) {
  // INFO("interrupt: err=%d\n", fr->err);
  if (e1000_inst) e1000_inst->fire(fr);
}

void e1000_init(void) {
  pci::device *e1000_dev = nullptr;
  pci::walk_devices([&](pci::device *dev) {
    // check if the device is an e1000 device
    if (dev->is_device(0x8086, 0x100e)) {
      e1000_dev = dev;
    }
  });

  if (e1000_dev != nullptr) {
    e1000_dev->enable_bus_mastering();

    e1000_inst = make_ref<e1000>(e1000_dev);

    if (!e1000_inst->start()) {
      e1000_inst = nullptr;
      INFO("failed!\n");
    } else {
      /*
      auto data = "hello";
      auto b = e1000_inst->send_packet(data, 6);
      printk("%d\n", b);
      */
    }
  }
}

module_init("e1000", e1000_init);
