#include "e1000.h"

#include <arch.h>
#include <lock.h>
#include <mem.h>
#include <module.h>
#include <net/net.h>
// #include <net/pkt.h>
#include <lwip/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
#include <lwip/snmp.h>
#include <lwip/tcpip.h>
#include <pci.h>
#include <phys.h>
#include <printf.h>
#include <sched.h>
#include <util.h>
#include <wait.h>

/**
 * This is the driver for the e1000 network card. Currently we only support a
 * single card at a time.
 */

static uintptr_t mem_base = 0;
static int has_eeprom = 0;
static uint8_t mac[6];
static int rx_index = 0;
static int tx_index = 0;

static pci::device *device;
struct net::interface *e1000_if = NULL;

static uint8_t *rx_virt[E1000_NUM_RX_DESC];
static uint8_t *tx_virt[E1000_NUM_TX_DESC];
static struct rx_desc *rx;
static struct tx_desc *tx;
static uintptr_t rx_phys;
static uintptr_t tx_phys;

// static list_t *net_queue = NULL;
static wait_queue e1000wait;
static struct netif e1000_netif;



static uint32_t mmio_read32(uintptr_t addr) {
  auto val = *((volatile uint32_t *)p2v(addr));
  // printf("read32(0x%p) -> 0x%08x\n", addr, val);
  return val;
}
static void mmio_write32(uintptr_t addr, uint32_t val) { *((volatile uint32_t *)p2v(addr)) = val; }

static void write_command(uint16_t addr, uint32_t val) { mmio_write32(mem_base + addr, val); }

static uint32_t read_command(uint16_t addr) { return mmio_read32(mem_base + addr); }

static int eeprom_detect(void) {
  write_command(E1000_REG_EEPROM, 1);

  for (int i = 0; i < 100000 && !has_eeprom; ++i) {
    uint32_t val = read_command(E1000_REG_EEPROM);
    if (val & 0x10) has_eeprom = 1;
  }

  return 0;
}

static uint16_t eeprom_read(uint8_t addr) {
  uint32_t temp = 0;
  write_command(E1000_REG_EEPROM, 1 | ((uint32_t)(addr) << 8));
  while (!((temp = read_command(E1000_REG_EEPROM)) & (1 << 4)))
    ;
  return (uint16_t)((temp >> 16) & 0xFFFF);
}

static void read_mac(void) {
  if (has_eeprom) {
    uint32_t t;
    t = eeprom_read(0);
    mac[0] = t & 0xFF;
    mac[1] = t >> 8;
    t = eeprom_read(1);
    mac[2] = t & 0xFF;
    mac[3] = t >> 8;
    t = eeprom_read(2);
    mac[4] = t & 0xFF;
    mac[5] = t >> 8;
  } else {
    uint8_t *mac_addr = (uint8_t *)p2v((mem_base + E1000_REG_RXADDR));
    for (int i = 0; i < 6; ++i) {
      mac[i] = mac_addr[i];
    }
  }
}

static void init_rx(void) {
  write_command(E1000_RDBAL, rx_phys & 0xFFFFFFFF);
  write_command(E1000_RDBAH, rx_phys >> 32);

  write_command(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(struct rx_desc));

  write_command(E1000_RDH, 0);
  write_command(E1000_RDT, E1000_NUM_RX_DESC - 1);

  rx_index = 0;

  write_command(E1000_REG_RCTRL, RCTL_SBP |       /* store bad packet */
                                     RCTL_UPE |   /* unicast promiscuous enable */
                                     RCTL_MPE |   /* multicast promiscuous enab */
                                     RCTL_SECRC | /* Strip Ethernet CRC */
                                     RCTL_LPE |   /* long packet enable */
                                     RCTL_BAM |   /* broadcast enable */
                                     E1000_RCTL_EN | 0);
}

static void init_tx(void) {
  auto base = (off_t)v2p(tx_phys);

  write_command(E1000_TDBAL, (uint32_t)(base & 0xffffffff));
  write_command(E1000_TDBAH, (uint32_t)(base >> 32));
  // tx descriptor length
  write_command(E1000_TDLEN, (uint32_t)(E1000_NUM_TX_DESC * sizeof(struct tx_desc)));
  // setup head/tail
  write_command(E1000_TDH, 0);
  write_command(E1000_TDT, 0);
  // set tx control register

  write_command(E1000_REG_TCTRL, TCTL_EN | TCTL_PSP | read_command(E1000_REG_TCTRL));

  tx_index = 0;
}

static void irq_handler(int i, reg_t *, void *) {
  uint32_t status = read_command(E1000_ICR);
  irq::eoi(i);

  if (status & E1000_ICR_LSC) {
    printf(KERN_INFO "[e1000]: status change\n");
  }

  if (status & E1000_ICR_RXT0) {
    /* receive packet */
    uint32_t rx_current = 0;
    for (;;) {
      rx_current = read_command(E1000_REG_RXDESCTAIL);
      if (rx_current == read_command(E1000_REG_RXDESCHEAD)) return;


      rx_current = (rx_current + 1) % E1000_NUM_RX_DESC;

      if (!(rx[rx_current].status & 1)) break;
      uint8_t *pbuf = (uint8_t *)rx_virt[rx_current];
      uint16_t plen = rx[rx_current].length;


      auto *pkt = pbuf_alloc(PBUF_RAW, plen, PBUF_RAM);
      memcpy(pkt->payload, pbuf, plen);

      e1000_netif.input(pkt, &e1000_netif);

      // auto pkt = net::pkt_buff::create(pbuf, plen);
      // net::packet_received(pkt);

      rx[rx_current].status = 0;

      write_command(E1000_REG_RXDESCTAIL, rx_current);
    }
  }

  // e1000wait.notify_all();
}

#define htonl(l) ((((l)&0xFF) << 24) | (((l)&0xFF00) << 8) | (((l)&0xFF0000) >> 8) | (((l)&0xFF000000) >> 24))
#define htons(s) ((((s)&0xFF) << 8) | (((s)&0xFF00) >> 8))
#define ntohl(l) htonl((l))
#define ntohs(s) htons((s))

int e1000_daemon(void *) {
  assert(device != NULL);

  while (1) {
    // e1000wait.wait();
  }
}



static spinlock send_lock;
static err_t e1000_netif_linkoutput(struct netif *netif, struct pbuf *p) {
  tx_index = read_command(E1000_REG_TXDESCTAIL);

  send_lock.lock();
  memcpy(tx_virt[tx_index], p->payload, p->len);

  auto &desc = tx[tx_index];
  desc.length = p->len;
  desc.cmd = CMD_EOP | CMD_IFCS | CMD_RS;  //| CMD_RPS;
  desc.status = 0;

  tx_index = (tx_index + 1) % E1000_NUM_TX_DESC;

  write_command(E1000_REG_TXDESCTAIL, tx_index);
  while (!(desc.status & 0x0f)) {
    arch_relax();
  }
  send_lock.unlock();

  return ERR_OK;
}

static err_t e1000_netif_init(struct netif *netif) {
  // struct device* dev = netif->state;

#if LWIP_NETIF_HOSTNAME
  netif->hostname = hostname;
#endif

  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100 * 1024 * 1024);

  netif->name[0] = 'e';
  netif->name[1] = 'n';

  netif->output = etharp_output;  // e1000_netif_output;
  netif->linkoutput = e1000_netif_linkoutput;

  // MAC address is 6 long
  netif->hwaddr_len = 6;
  memcpy(netif->hwaddr, mac, 6);

  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHERNET;


  // dev->net.low_level_init(dev->net.internals, dev->net.address, NULL);

  return ERR_OK;
}



void send_data(void *data, size_t size) {}


int e1000_init_thread(void *) {
  pci::device *e1000_dev = nullptr;
  pci::walk_devices([&](pci::device *dev) {
    // check if the device is an e1000 device
    if (dev->is_device(0x8086, 0x100e)) {
      e1000_dev = dev;
    }
  });

  if (e1000_dev == nullptr) {
    return 0;
  }
  device = e1000_dev;

  device->enable_bus_mastering();

  mem_base = (unsigned long)device->get_bar(0).raw;

  eeprom_detect();
  printf(KERN_INFO "[e1000]: has_eeprom = %d\n", has_eeprom);
  read_mac();

  printf(KERN_INFO "[e1000]: device mac %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  rx = (struct rx_desc *)phys::kalloc(NPAGES(sizeof(struct rx_desc) * E1000_NUM_RX_DESC + 16));

  rx_phys = (unsigned long)v2p(rx);
  for (int i = 0; i < E1000_NUM_RX_DESC; ++i) {
    rx_virt[i] = (unsigned char *)phys::kalloc(NPAGES(8192 + 16));
    rx[i].addr = (unsigned long)v2p(rx_virt[i]);
    rx[i].status = 0;
  }

  tx = (struct tx_desc *)phys::kalloc(NPAGES(sizeof(struct tx_desc) * E1000_NUM_TX_DESC + 16));
  tx_phys = (unsigned long)v2p(tx);

  for (int i = 0; i < E1000_NUM_TX_DESC; ++i) {
    tx_virt[i] = (unsigned char *)phys::kalloc(NPAGES(8192 + 16));
    tx[i].addr = (unsigned long)v2p(tx_virt[i]);
    tx[i].status = 0;
    tx[i].length = 0;
    tx[i].cmd = (1 << 0);
  }

  uint32_t ctrl = read_command(E1000_REG_CTRL);
  /* reset phy */
  write_command(E1000_REG_CTRL, ctrl | (0x80000000));
  read_command(E1000_REG_STATUS);

  /* reset mac */
  write_command(E1000_REG_CTRL, ctrl | (0x04000000));
  read_command(E1000_REG_STATUS);

  /* Reload EEPROM */
  write_command(E1000_REG_CTRL, ctrl | (0x00002000));
  read_command(E1000_REG_STATUS);

  /* initialize */
  write_command(E1000_REG_CTRL, ctrl | (1 << 26));

  sched::dumb_sleepticks(1);

  uint32_t status = read_command(E1000_REG_CTRL);
  status |= (1 << 5);       /* set auto speed detection */
  status |= (1 << 6);       /* set link up */
  status &= ~(1 << 3);      /* unset link reset */
  status &= ~(1UL << 31UL); /* unset phy reset */
  status &= ~(1 << 7);      /* unset invert loss-of-signal */
  write_command(E1000_REG_CTRL, status);

  /* Disables flow control */
  write_command(0x0028, 0);
  write_command(0x002c, 0);
  write_command(0x0030, 0);
  write_command(0x0170, 0);

  /* Unset flow control */
  status = read_command(E1000_REG_CTRL);
  status &= ~(1 << 30);
  write_command(E1000_REG_CTRL, status);

  // grab the irq
  auto e1000_irq = device->interrupt;
  irq::install(e1000_irq, irq_handler, "e1000");

  for (int i = 0; i < 128; ++i)
    write_command(0x5200 + i * 4, 0);
  for (int i = 0; i < 64; ++i)
    write_command(0x4000 + i * 4, 0);

  /* setup interrupts */
  write_command(E1000_IMS, 0xFF);
  read_command(E1000_ICR);

  write_command(E1000_REG_RCTRL, (1 << 4));

  init_rx();
  init_tx();

  write_command(E1000_CTL, read_command(E1000_CTL) | E1000_CTL_SLU);

  int link_is_up = (read_command(E1000_REG_STATUS) & (1 << 1));
  printf(KERN_INFO "[e1000]: done. has_eeprom = %d, link is up = %d, irq=%d\n", has_eeprom, link_is_up, e1000_irq);

  ip4_addr_t ip; /* Ipv4 Address */
  ip4_addr_t nm; /* net mask */
  ip4_addr_t gw; /* Gateway */

  // interface = new e1000_interface();
  IP4_ADDR(&ip, 10, 0, 2, 15);
  IP4_ADDR(&nm, 255, 255, 255, 0);
  IP4_ADDR(&gw, 10, 0, 2, 2);


  netif_add(&e1000_netif, &ip, &nm, &gw, NULL, e1000_netif_init, tcpip_input);
  netif_set_up(&e1000_netif);
  netif_set_link_up(&e1000_netif);
  while (netif_is_up(&e1000_netif) == 0) {
    arch_relax();
  }

  netif_set_default(&e1000_netif);


  dhcp_start(&e1000_netif);
  while (!dhcp_supplied_address(&e1000_netif)) {
    arch_relax();
  }

  printf("E1000 DONE!\n");
  return 0;
}


void e1000_init(void) {
  e1000_init_thread(NULL);
  /* Defer this to a thread... */
  // sched::proc::create_kthread("[e1000 init thread]", e1000_init_thread, NULL);
}

module_init("e1000", e1000_init);
