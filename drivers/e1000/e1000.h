#ifndef __e1000__
#define __e1000__

#include <types.h>

#define E1000_REG_CTRL 0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_EEPROM 0x0014
#define E1000_REG_CTRL_EXT 0x0018

#define E1000_REG_RCTRL 0x0100
#define E1000_REG_RXDESCLO 0x2800
#define E1000_REG_RXDESCHI 0x2804
#define E1000_REG_RXDESCLEN 0x2808
#define E1000_REG_RXDESCHEAD 0x2810
#define E1000_REG_RXDESCTAIL 0x2818

#define E1000_REG_TCTRL 0x0400
#define E1000_REG_TXDESCLO 0x3800
#define E1000_REG_TXDESCHI 0x3804
#define E1000_REG_TXDESCLEN 0x3808
#define E1000_REG_TXDESCHEAD 0x3810
#define E1000_REG_TXDESCTAIL 0x3818

#define E1000_REG_RXADDR 0x5400

#define E1000_NUM_RX_DESC 16
#define E1000_NUM_TX_DESC 16

struct tx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint8_t cso;
  volatile uint8_t cmd;
  volatile uint8_t status;
  volatile uint8_t css;
  volatile uint16_t special;
} __attribute__((packed));

struct rx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint16_t checksum;
  volatile uint8_t status;
  volatile uint8_t errors;
  volatile uint16_t special;
} __attribute__((packed)); /* this looks like it should pack fine as-is */

#define RCTL_EN (1 << 1)         /* Receiver Enable */
#define RCTL_SBP (1 << 2)        /* Store Bad Packets */
#define RCTL_UPE (1 << 3)        /* Unicast Promiscuous Enabled */
#define RCTL_MPE (1 << 4)        /* Multicast Promiscuous Enabled */
#define RCTL_LPE (1 << 5)        /* Long Packet Reception Enable */
#define RCTL_LBM_NONE (0 << 6)   /* No Loopback */
#define RCTL_LBM_PHY (3 << 6)    /* PHY or external SerDesc loopback */
#define RTCL_RDMTS_HALF (0 << 8) /* Free Buffer Threshold is 1/2 of RDLEN */
#define RTCL_RDMTS_QUARTER                                                  \
  (1 << 8)                         /* Free Buffer Threshold is 1/4 of RDLEN \
                                    */
#define RTCL_RDMTS_EIGHTH (2 << 8) /* Free Buffer Threshold is 1/8 of RDLEN */
#define RCTL_MO_36 (0 << 12)       /* Multicast Offset - bits 47:36 */
#define RCTL_MO_35 (1 << 12)       /* Multicast Offset - bits 46:35 */
#define RCTL_MO_34 (2 << 12)       /* Multicast Offset - bits 45:34 */
#define RCTL_MO_32 (3 << 12)       /* Multicast Offset - bits 43:32 */
#define RCTL_BAM (1 << 15)         /* Broadcast Accept Mode */
#define RCTL_VFE (1 << 18)         /* VLAN Filter Enable */
#define RCTL_CFIEN (1 << 19)       /* Canonical Form Indicator Enable */
#define RCTL_CFI (1 << 20)         /* Canonical Form Indicator Bit Value */
#define RCTL_DPF (1 << 22)         /* Discard Pause Frames */
#define RCTL_PMCF (1 << 23)        /* Pass MAC Control Frames */
#define RCTL_SECRC (1 << 26)       /* Strip Ethernet CRC */

#define RCTL_BSIZE_256 (3 << 16)
#define RCTL_BSIZE_512 (2 << 16)
#define RCTL_BSIZE_1024 (1 << 16)
#define RCTL_BSIZE_2048 (0 << 16)
#define RCTL_BSIZE_4096 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384 ((1 << 16) | (1 << 25))

#define TCTL_EN (1 << 1)      /* Transmit Enable */
#define TCTL_PSP (1 << 3)     /* Pad Short Packets */
#define TCTL_CT_SHIFT 4       /* Collision Threshold */
#define TCTL_COLD_SHIFT 12    /* Collision Distance */
#define TCTL_SWXOFF (1 << 22) /* Software XOFF Transmission */
#define TCTL_RTLC (1 << 24)   /* Re-transmit on Late Collision */

#define CMD_EOP (1 << 0)  /* End of Packet */
#define CMD_IFCS (1 << 1) /* Insert FCS */
#define CMD_IC (1 << 2)   /* Insert Checksum */
#define CMD_RS (1 << 3)   /* Report Status */
#define CMD_RPS (1 << 4)  /* Report Packet Sent */
#define CMD_VLE (1 << 6)  /* VLAN Packet Enable */
#define CMD_IDE (1 << 7)  /* Interrupt Delay Enable */

#define E1000_ICR_MSG_TXDW "Transmit desc written back"
#define E1000_ICR_MSG_TXQE "Transmit Queue empty"
#define E1000_ICR_MSG_LSC "Link Status Change"
#define E1000_ICR_MSG_RXSEQ "rx sequence error"
#define E1000_ICR_MSG_RXDMT0 "rx desc min. threshold (0)"
#define E1000_ICR_MSG_RXO "rx overrun"
#define E1000_ICR_MSG_RXT0 "rx timer intr (ring 0)"
#define E1000_ICR_MSG_MDAC "MDIO access complete"
#define E1000_ICR_MSG_RXCFG "RX /c/ ordered set"
#define E1000_ICR_MSG_GPI_EN0 "GP Int 0"
#define E1000_ICR_MSG_GPI_EN1 "GP Int 1"
#define E1000_ICR_MSG_GPI_EN2 "GP Int 2"
#define E1000_ICR_MSG_GPI_EN3 "GP Int 3"
#define E1000_ICR_MSG_TXD_LOW "TX Desc Low"
#define E1000_ICR_MSG_SRPD "SRPD"
#define E1000_ICR_MSG_ACK "Receive Ack frame"
#define E1000_ICR_MSG_MNG "Manageability event"
#define E1000_ICR_MSG_DOCK "Dock/Undock"
#define E1000_ICR_MSG_INT_ASSERTED "If this bit asserted, the driver should claim the interrupt"

/* Interrupt Cause Read */
#define E1000_ICR_TXDW 0x00000001    /* Transmit desc written back */
#define E1000_ICR_TXQE 0x00000002    /* Transmit Queue empty */
#define E1000_ICR_LSC 0x00000004     /* Link Status Change */
#define E1000_ICR_RXSEQ 0x00000008   /* rx sequence error */
#define E1000_ICR_RXDMT0 0x00000010  /* rx desc min. threshold (0) */
#define E1000_ICR_RXO 0x00000040     /* rx overrun */
#define E1000_ICR_RXT0 0x00000080    /* rx timer intr (ring 0) */
#define E1000_ICR_MDAC 0x00000200    /* MDIO access complete */
#define E1000_ICR_RXCFG 0x00000400   /* RX /c/ ordered set */
#define E1000_ICR_GPI_EN0 0x00000800 /* GP Int 0 */
#define E1000_ICR_GPI_EN1 0x00001000 /* GP Int 1 */
#define E1000_ICR_GPI_EN2 0x00002000 /* GP Int 2 */
#define E1000_ICR_GPI_EN3 0x00004000 /* GP Int 3 */
#define E1000_ICR_TXD_LOW 0x00008000
#define E1000_ICR_SRPD 0x00010000
#define E1000_ICR_ACK 0x00020000  /* Receive Ack frame */
#define E1000_ICR_MNG 0x00040000  /* Manageability event */
#define E1000_ICR_DOCK 0x00080000 /* Dock/Undock */
#define E1000_ICR_INT_ASSERTED \
  0x80000000 /* If this bit asserted, the driver should claim the interrupt */
#define E1000_ICR_RXD_FIFO_PAR0 0x00100000 /* queue 0 Rx descriptor FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR0 0x00200000 /* queue 0 Tx descriptor FIFO parity error */
#define E1000_ICR_HOST_ARB_PAR 0x00400000  /* host arb read buffer parity error */
#define E1000_ICR_PB_PAR 0x00800000        /* packet buffer parity error */
#define E1000_ICR_RXD_FIFO_PAR1 0x01000000 /* queue 1 Rx descriptor FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR1 0x02000000 /* queue 1 Tx descriptor FIFO parity error */
#define E1000_ICR_ALL_PARITY 0x03F00000    /* all parity error bits */
#define E1000_ICR_DSW 0x00000020           /* FW changed the status of DISSW bit in the FWSM */
#define E1000_ICR_PHYINT 0x00001000        /* LAN connected device generates an interrupt */
#define E1000_ICR_EPRST 0x00100000         /* ME hardware reset occurs */

/* Registers */
#define E1000_CTL (0x0000)   /* Device Control Register - RW */
#define E1000_EERD (0x0014)  /* EEPROM Read - RW */
#define E1000_ICR (0x00C0)   /* Interrupt Cause Read - R */
#define E1000_IMS (0x00D0)   /* Interrupt Mask Set - RW */
#define E1000_IMC (0x00D8)   /* Interrupt Mask Clear - RW */
#define E1000_RCTL (0x0100)  /* RX Control - RW */
#define E1000_TCTL (0x0400)  /* TX Control - RW */
#define E1000_TIPG (0x0410)  /* TX Inter-packet gap -RW */
#define E1000_RDBAL (0x2800) /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH (0x2804) /* RX Descriptor Base Address High - RW */
#define E1000_RDTR (0x2820)  /* RX Delay Timer */
#define E1000_RADV (0x282C)  /* RX Interrupt Absolute Delay Timer */
#define E1000_RDH (0x2810)   /* RX Descriptor Head - RW */
#define E1000_RDT (0x2818)   /* RX Descriptor Tail - RW */
#define E1000_RDLEN (0x2808) /* RX Descriptor Length - RW */
#define E1000_RSRPD (0x2C00) /* RX Small Packet Detect Interrupt */
#define E1000_TDBAL (0x3800) /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH (0x3804) /* TX Descriptor Base Address Hi - RW */
#define E1000_TDLEN (0x3808) /* TX Descriptor Length - RW */
#define E1000_TDH (0x3810)   /* TX Descriptor Head - RW */
#define E1000_TDT (0x3818)   /* TX Descripotr Tail - RW */
#define E1000_MTA (0x5200)   /* Multicast Table Array - RW Array */
#define E1000_RA (0x5400)    /* Receive Address - RW Array */

/* Device Control */
#define E1000_CTL_SLU 0x00000040     /* set link up */
#define E1000_CTL_FRCSPD 0x00000800  /* force speed */
#define E1000_CTL_FRCDPLX 0x00001000 /* force duplex */
#define E1000_CTL_RST 0x00400000     /* full reset */

/* EEPROM */
#define E1000_EERD_ADDR 8        /* num of bit shifts to get to addr section */
#define E1000_EERD_DATA 16       /* num of bit shifts to get to data section */
#define E1000_EERD_READ (1 << 0) /* 0th bit */
#define E1000_EERD_DONE (1 << 4) /* 4th bit */

/* Transmit Control */
#define E1000_TCTL_RST 0x00000001 /* software reset */
#define E1000_TCTL_EN 0x00000002  /* enable tx */
#define E1000_TCTL_BCE 0x00000004 /* busy check enable */
#define E1000_TCTL_PSP 0x00000008 /* pad short packets */
#define E1000_TCTL_CT 0x00000ff0  /* collision threshold */
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD 0x003ff000 /* collision distance */
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TCTL_SWXOFF 0x00400000 /* SW Xoff transmission */
#define E1000_TCTL_PBE 0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC 0x01000000   /* Re-transmit on late collision */
#define E1000_TCTL_NRTU 0x02000000   /* No Re-transmit on underrun */
#define E1000_TCTL_MULR 0x10000000   /* Multiple request support */

/* Receive Control */
#define E1000_RCTL_RST 0x00000001         /* Software reset */
#define E1000_RCTL_EN 0x00000002          /* enable */
#define E1000_RCTL_SBP 0x00000004         /* store bad packet */
#define E1000_RCTL_UPE 0x00000008         /* unicast promiscuous enable */
#define E1000_RCTL_MPE 0x00000010         /* multicast promiscuous enab */
#define E1000_RCTL_LPE 0x00000020         /* long packet enable */
#define E1000_RCTL_LBM_NO 0x00000000      /* no loopback mode */
#define E1000_RCTL_LBM_MAC 0x00000040     /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP 0x00000080     /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR 0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK 0x00000C00   /* Descriptor type mask */
#define E1000_RCTL_DTYP_PS 0x00000400     /* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF 0x00000000  /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT 0x00000100  /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH 0x00000200 /* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT 12            /* multicast offset shift */
#define E1000_RCTL_MO_0 0x00000000        /* multicast offset 11:0 */
#define E1000_RCTL_MO_1 0x00001000        /* multicast offset 12:1 */
#define E1000_RCTL_MO_2 0x00002000        /* multicast offset 13:2 */
#define E1000_RCTL_MO_3 0x00003000        /* multicast offset 15:4 */
#define E1000_RCTL_MDR 0x00004000         /* multicast desc ring 0 */
#define E1000_RCTL_BAM 0x00008000         /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048 0x00000000 /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024 0x00010000 /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512 0x00020000  /* rx buffer size 512 */
#define E1000_RCTL_SZ_256 0x00030000  /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384 0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192 0x00020000     /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096 0x00030000     /* rx buffer size 4096 */
#define E1000_RCTL_VFE 0x00040000         /* vlan filter enable */
#define E1000_RCTL_CFIEN 0x00080000       /* canonical form enable */
#define E1000_RCTL_CFI 0x00100000         /* canonical form indicator */
#define E1000_RCTL_DPF 0x00400000         /* discard pause frames */
#define E1000_RCTL_PMCF 0x00800000        /* pass MAC control frames */
#define E1000_RCTL_BSEX 0x02000000        /* Buffer size extension */
#define E1000_RCTL_SECRC 0x04000000       /* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK 0x78000000 /* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT 27        /* Flexible buffer shift */

#define DATA_MAX 1518

/* Transmit Descriptor command definitions [E1000 3.3.3.1] */
#define E1000_TXD_CMD_EOP 0x01  /* End of Packet */
#define E1000_TXD_CMD_IFCS 0x02 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_RS 0x08   /* Report Status */
#define E1000_TXD_CMD_DEXT 0x20 /* Descriptor extension (0 = legacy) */

/* Transmit Descriptor status definitions [E1000 3.3.3.2] */
#define E1000_TXD_STAT_DD 0x00000001 /* Descriptor Done */

#endif
