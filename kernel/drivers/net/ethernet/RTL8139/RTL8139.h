#include <module.h>
#include <pci.h>


#define REG_MAC 0x00
#define REG_MAR0 0x08
#define REG_MAR4 0x12
#define REG_TXSTATUS0 0x10
#define REG_TXADDR0 0x20
#define REG_RXBUF 0x30
#define REG_COMMAND 0x37
#define REG_CAPR 0x38
#define REG_IMR 0x3C
#define REG_ISR 0x3E
#define REG_TXCFG 0x40
#define REG_RXCFG 0x44
#define REG_MPC 0x4C
#define REG_CFG9346 0x50
#define REG_CONFIG1 0x52
#define REG_MSR 0x58
#define REG_BMCR 0x62

#define TX_STATUS_OWN 0x2000
#define TX_STATUS_THRESHOLD_MAX 0x3F0000

#define COMMAND_RX_EMPTY 0x01
#define COMMAND_TX_ENABLE 0x04
#define COMMAND_RX_ENABLE 0x08
#define COMMAND_RESET 0x10

#define INT_RXOK 0x01
#define INT_RXERR 0x02
#define INT_TXOK 0x04
#define INT_TXERR 0x08
#define INT_RX_BUFFER_OVERFLOW 0x10
#define INT_LINK_CHANGE 0x20
#define INT_RX_FIFO_OVERFLOW 0x40
#define INT_LENGTH_CHANGE 0x2000
#define INT_SYSTEM_ERROR 0x8000

#define CFG9346_NONE 0x00
#define CFG9346_EEM0 0x40
#define CFG9346_EEM1 0x80

#define TXCFG_TXRR_ZERO 0x00
#define TXCFG_MAX_DMA_16B 0x000
#define TXCFG_MAX_DMA_32B 0x100
#define TXCFG_MAX_DMA_64B 0x200
#define TXCFG_MAX_DMA_128B 0x300
#define TXCFG_MAX_DMA_256B 0x400
#define TXCFG_MAX_DMA_512B 0x500
#define TXCFG_MAX_DMA_1K 0x600
#define TXCFG_MAX_DMA_2K 0x700
#define TXCFG_IFG11 0x3000000

#define RXCFG_AAP 0x01
#define RXCFG_APM 0x02
#define RXCFG_AM 0x04
#define RXCFG_AB 0x08
#define RXCFG_AR 0x10
#define RXCFG_WRAP_INHIBIT 0x80
#define RXCFG_MAX_DMA_16B 0x000
#define RXCFG_MAX_DMA_32B 0x100
#define RXCFG_MAX_DMA_64B 0x200
#define RXCFG_MAX_DMA_128B 0x300
#define RXCFG_MAX_DMA_256B 0x400
#define RXCFG_MAX_DMA_512B 0x500
#define RXCFG_MAX_DMA_1K 0x600
#define RXCFG_MAX_DMA_UNLIMITED 0x0700
#define RXCFG_RBLN_8K 0x0000
#define RXCFG_RBLN_16K 0x0800
#define RXCFG_RBLN_32K 0x1000
#define RXCFG_RBLN_64K 0x1800
#define RXCFG_FTH_NONE 0xE000

#define MSR_LINKB 0x02
#define MSR_RX_FLOW_CONTROL_ENABLE 0x40

#define BMCR_SPEED 0x2000
#define BMCR_AUTO_NEGOTIATE 0x1000
#define BMCR_DUPLEX 0x0100

#define RX_MULTICAST 0x8000
#define RX_PHYSICAL_MATCH 0x4000
#define RX_BROADCAST 0x2000
#define RX_INVALID_SYMBOL_ERROR 0x20
#define RX_RUNT 0x10
#define RX_LONG 0x08
#define RX_CRC_ERROR 0x04
#define RX_FRAME_ALIGNMENT_ERROR 0x02
#define RX_OK 0x01

#define PACKET_SIZE_MAX 0x600
#define PACKET_SIZE_MIN 0x16

#define RX_BUFFER_SIZE 32768
#define TX_BUFFER_SIZE PACKET_SIZE_MAX


#define RTL8139_TX_BUFFER_COUNT 4



class rtl8139 /* TODO: superclass for network adapter */ {
  public:
    rtl8139(pci::device &, u8 irq);
    ~rtl8139();



    void out8(u16 address, u8 data);
    void out16(u16 address, u16 data);
    void out32(u16 address, u32 data);
    u8 in8(u16 address);
    u16 in16(u16 address);
    u32 in32(u16 address);


    void send_raw(const void* data, int length);


    void reset();
    void read_mac_address();
    void receive();

    void handle_irq();

  private:
    pci::device &m_dev;

    u16 m_io_base = 0;
    u8 m_interrupt_line = 0;
    u64 m_rx_buffer_addr = 0;
    u16 m_rx_buffer_offset = 0;
    u64 m_tx_buffer_addr[RTL8139_TX_BUFFER_COUNT];
    u8 m_tx_next_buffer = 0;
    void *m_packet_buffer;

    u8 mac[6];
};
