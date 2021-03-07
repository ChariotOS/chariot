#pragma once
#include <lock.h>
#include <pci.h>
#include <sem.h>
/* This file defines constants for XHCI USB controllers */
#include <types.h>

// PCI IDs
#define PCI_VENDOR_INTEL 0x8086
#define PCI_DEVICE_INTEL_PANTHER_POINT_XHCI 0x1e31
#define PCI_DEVICE_INTEL_LYNX_POINT_XHCI 0x8c31
#define PCI_DEVICE_INTEL_LYNX_POINT_LP_XHCI 0x9c31
#define PCI_DEVICE_INTEL_BAYTRAIL_XHCI 0x0f35
#define PCI_DEVICE_INTEL_WILDCAT_POINT_XHCI 0x8cb1
#define PCI_DEVICE_INTEL_WILDCAT_POINT_LP_XHCI 0x9cb1

// Intel quirks registers in PCI config
#define XHCI_INTEL_USB3PRM 0xdc     // USB 3.0 Port Routing Mask
#define XHCI_INTEL_USB3_PSSEN 0xd8  // USB 3.0 Port SuperSpeed Enable
#define XHCI_INTEL_USB2PRM 0xd4     // USB 2.0 Port Routing Mask
#define XHCI_INTEL_XUSB2PR 0xd0     // USB 2.0 Port Routing

// Host Controller Capability Registers
#define XHCI_HCI_CAPLENGTH 0x00  // HCI Capability Register Length
#define HCI_CAPLENGTH(p) (((p) >> 0) & 0xff)
#define XHCI_HCI_VERSION 0x00  // HCI Interface Version Number
#define HCI_VERSION(p) (((p) >> 16) & 0xffff)
#define XHCI_HCSPARAMS1 0x04  // Structural Parameters 1
// HCSPARAMS1
#define HCS_MAX_SLOTS(p) (((p) >> 0) & 0xff)
#define HCS_MAX_PORTS(p) (((p) >> 24) & 0xff)
#define XHCI_HCSPARAMS2 0x08  // Structural Parameters 2
#define HCS_IST(p) (((p) >> 0) & 0xf)
#define HCS_ERST_MAX(p) (((p) >> 4) & 0xf)
#define HCS_SPR(p) (((p) >> 26) & 0x1)
#define HCS_MAX_SC_BUFFERS(p) (((((p) >> 21) & 0x1f) << 5) | (((p) >> 27) & 0x1f))
#define XHCI_HCSPARAMS3 0x0C  // Structural Parameters 3
#define HCS_U1_DEVICE_LATENCY(p) (((p) >> 0) & 0xff)
#define HCS_U2_DEVICE_LATENCY(p) (((p) >> 16) & 0xffff)
#define XHCI_HCCPARAMS 0x10  // Capability Parameters
#define HCC_AC64(p) (((p) >> 0) & 0x1)
#define HCC_BNC(p) (((p) >> 1) & 0x1)
#define HCC_CSZ(p) (((p) >> 2) & 0x1)
#define HCC_PPC(p) (((p) >> 3) & 0x1)
#define HCC_PIND(p) (((p) >> 4) & 0x1)
#define HCC_LHRC(p) (((p) >> 5) & 0x1)
#define HCC_LTC(p) (((p) >> 6) & 0x1)
#define HCC_NSS(p) (((p) >> 7) & 0x1)
#define HCC_PAE(p) (((p) >> 8) & 0x1)
#define HCC_SPC(p) (((p) >> 9) & 0x1)
#define HCC_SEC(p) (((p) >> 10) & 0x1)
#define HCC_CFC(p) (((p) >> 11) & 0x1)
#define HCC_MAXPSASIZE(p) (((p) >> 12) & 0xf)
#define HCC_XECP(p) (((p) >> 16) & 0xfff)
#define XHCI_DBOFF 0x14   // Doorbell Register offset
#define XHCI_RTSOFF 0x18  // Runtime Register Space offset


// Host Controller Operational Registers
#define XHCI_CMD 0x00  // USB Command
// USB Command Register
#define CMD_RUN (1 << 0)
#define CMD_HCRST (1 << 1)   // Host Controller Reset
#define CMD_INTE (1 << 2)    // IRQ Enable
#define CMD_HSEE (1 << 3)    // Host System Error En
#define CMD_LHCRST (1 << 7)  // Light Host Controller Reset
#define CMD_CSS (1 << 8)     // Controller Save State
#define CMD_CRS (1 << 9)     // Controller Restore State
#define CMD_EWE (1 << 10)    // Enable Wrap Event

#define XHCI_STS 0x04  // USB Status
// USB Status Register
#define STS_HCH (1 << 0)   // Host Controller Halt
#define STS_HSE (1 << 2)   // Host System Error
#define STS_EINT (1 << 3)  // Event Interrupt
#define STS_PCD (1 << 4)   // Port Change Detect
#define STS_SSS (1 << 8)   // Save State Status
#define STS_RSS (1 << 9)   // Restore State Status
#define STS_SRE (1 << 10)  // Save Restore Error
#define STS_CNR (1 << 11)  // Controller Not Ready
#define STS_HCE (1 << 12)  // Host Controller Error

#define XHCI_PAGESIZE 0x08  // PAGE SIZE
#define XHCI_DNCTRL 0x14
// Section 5.4.5
#define XHCI_CRCR_LO 0x18
#define XHCI_CRCR_HI 0x1C
#define CRCR_RCS (1 << 0)
#define CRCR_CS (1 << 1)
#define CRCR_CA (1 << 2)
#define CRCR_CRR (1 << 3)
// Section 5.4.6
#define XHCI_DCBAAP_LO 0x30
#define XHCI_DCBAAP_HI 0x34
// Section 5.4.7
#define XHCI_CONFIG 0x38


// Host Controller Runtime Registers
#define XHCI_MFINDEX 0x0000
// Section 5.5.2.1
#define XHCI_IMAN(n) (0x0020 + (0x20 * (n)))
// IMAN
#define IMAN_INTR_ENA 0x00000002
// Section 5.5.2.2
#define XHCI_IMOD(n) (0x0024 + (0x20 * (n)))
// Section 5.5.2.3.1
#define XHCI_ERSTSZ(n) (0x0028 + (0x20 * (n)))
// ERSTSZ
#define XHCI_ERSTS_SET(x) ((x)&0xFFFF)
// Section 5.5.2.3.2
#define XHCI_ERSTBA_LO(n) (0x0030 + (0x20 * (n)))
#define XHCI_ERSTBA_HI(n) (0x0034 + (0x20 * (n)))
// Section 5.5.2.3.3
#define XHCI_ERDP_LO(n) (0x0038 + (0x20 * (n)))
#define XHCI_ERDP_HI(n) (0x003C + (0x20 * (n)))
// Event Handler Busy (EHB)
#define ERDP_BUSY (1 << 3)


// Host Controller Doorbell Registers
#define XHCI_DOORBELL(n) (0x0000 + (4 * (n)))
#define XHCI_DOORBELL_TARGET(x) ((x)&0xff)
#define XHCI_DOORBELL_TARGET_GET(x) ((x)&0xff)
#define XHCI_DOORBELL_STREAMID(x) (((x)&0xffff) << 16)
#define XHCI_DOORBELL_STREAMID_GET(x) (((x) >> 16) & 0xffff)


// Extended Capabilities
#define XECP_ID(x) ((x)&0xff)
#define HCS0_XECP(x) (((x) >> 16) & 0xffff)
#define XECP_NEXT(x) (((x) >> 8) & 0xff)
#define XHCI_LEGSUP_CAPID 0x01
#define XHCI_LEGSUP_OSOWNED (1 << 24)    // OS Owned Semaphore
#define XHCI_LEGSUP_BIOSOWNED (1 << 16)  // BIOS Owned Semaphore

#define XHCI_LEGCTLSTS 0x04
#define XHCI_LEGCTLSTS_DISABLE_SMI ((0x7 << 1) + (0xff << 5) + (0x7 << 17))
#define XHCI_LEGCTLSTS_EVENTS_SMI (0x7 << 29)

#define XHCI_SUPPORTED_PROTOCOLS_CAPID 0x02
#define XHCI_SUPPORTED_PROTOCOLS_0_MINOR(x) (((x) >> 16) & 0xff)
#define XHCI_SUPPORTED_PROTOCOLS_0_MAJOR(x) (((x) >> 24) & 0xff)

#define XHCI_SUPPORTED_PROTOCOLS_1_COUNT(x) (((x) >> 8) & 0xff)
#define XHCI_SUPPORTED_PROTOCOLS_1_OFFSET(x) (((x) >> 0) & 0xff)


// Port status Registers
// Section 5.4.8
#define XHCI_PORTSC(n) (0x400 + (0x10 * (n)))
#define PS_CCS (1 << 0)
#define PS_PED (1 << 1)
#define PS_OCA (1 << 3)
#define PS_PR (1 << 4)
#define PS_PP (1 << 9)
#define PS_SPEED_GET(x) (((x) >> 10) & 0xF)
#define PS_LWS (1 << 16)
#define PS_CSC (1 << 17)
#define PS_PEC (1 << 18)
#define PS_WRC (1 << 19)
#define PS_OCC (1 << 20)
#define PS_PRC (1 << 21)
#define PS_PLC (1 << 22)
#define PS_CEC (1 << 23)
#define PS_CAS (1 << 24)
#define PS_WCE (1 << 25)
#define PS_WDE (1 << 26)
#define PS_WPR (1 << 30)

#define PS_CLEAR 0x80FF00F7U

#define PS_PLS_MASK (0xf << 5)
#define PS_XDEV_U0 (0x0 << 5)
#define PS_XDEV_U3 (0x3 << 5)


// Completion Code
#define TRB_2_COMP_CODE_GET(x) (((x) >> 24) & 0xff)
#define COMP_INVALID 0
#define COMP_SUCCESS 1
#define COMP_DATA_BUFFER 2
#define COMP_BABBLE 3
#define COMP_USB_TRANSACTION 4
#define COMP_TRB 5
#define COMP_STALL 6
#define COMP_RESOURCE 7
#define COMP_BANDWIDTH 8
#define COMP_NO_SLOTS 9
#define COMP_INVALID_STREAM 10
#define COMP_SLOT_NOT_ENABLED 11
#define COMP_ENDPOINT_NOT_ENABLED 12
#define COMP_SHORT_PACKET 13
#define COMP_RING_UNDERRUN 14
#define COMP_RING_OVERRUN 15
#define COMP_VF_RING_FULL 16
#define COMP_PARAMETER 17
#define COMP_BANDWIDTH_OVERRUN 18
#define COMP_CONTEXT_STATE 19
#define COMP_NO_PING_RESPONSE 20
#define COMP_EVENT_RING_FULL 21
#define COMP_INCOMPATIBLE_DEVICE 22
#define COMP_MISSED_SERVICE 23
#define COMP_COMMAND_RING_STOPPED 24
#define COMP_COMMAND_ABORTED 25
#define COMP_STOPPED 26
#define COMP_LENGTH_INVALID 27
#define COMP_MAX_EXIT_LATENCY 29
#define COMP_ISOC_OVERRUN 31
#define COMP_EVENT_LOST 32
#define COMP_UNDEFINED 33
#define COMP_INVALID_STREAM_ID 34
#define COMP_SECONDARY_BANDWIDTH 35
#define COMP_SPLIT_TRANSACTION 36

#define TRB_2_TD_SIZE(x) (((x)&0x1F) << 17)
#define TRB_2_TD_SIZE_GET(x) (((x) >> 17) & 0x1F)
#define TRB_2_REM(x) ((x)&0xFFFFFF)
#define TRB_2_REM_GET(x) ((x)&0xFFFFFF)
#define TRB_2_BYTES(x) ((x)&0x1FFFF)
#define TRB_2_BYTES_GET(x) ((x)&0x1FFFF)
#define TRB_2_IRQ(x) (((x)&0x3FF) << 22)
#define TRB_2_IRQ_GET(x) (((x) >> 22) & 0x3FF)
#define TRB_2_STREAM(x) (((x)&0xFF) << 16)
#define TRB_2_STREAM_GET(x) (((x) >> 16) & 0xFF)

#define TRB_3_TYPE(x) (((x)&0x3F) << 10)
#define TRB_3_TYPE_GET(x) (((x) >> 10) & 0x3F)
// TRB Type (table 131)
#define TRB_TYPE_NORMAL 1
#define TRB_TYPE_SETUP_STAGE 2
#define TRB_TYPE_DATA_STAGE 3
#define TRB_TYPE_STATUS_STAGE 4
#define TRB_TYPE_ISOCH 5
#define TRB_TYPE_LINK 6
#define TRB_TYPE_EVENT_DATA 7
#define TRB_TYPE_TR_NOOP 8
// commands
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_DISABLE_SLOT 10
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_CONFIGURE_ENDPOINT 12
#define TRB_TYPE_EVALUATE_CONTEXT 13
#define TRB_TYPE_RESET_ENDPOINT 14
#define TRB_TYPE_STOP_ENDPOINT 15
#define TRB_TYPE_SET_TR_DEQUEUE 16
#define TRB_TYPE_RESET_DEVICE 17
#define TRB_TYPE_FORCE_EVENT 18
#define TRB_TYPE_NEGOCIATE_BW 19
#define TRB_TYPE_SET_LATENCY_TOLERANCE 20
#define TRB_TYPE_GET_PORT_BW 21
#define TRB_TYPE_FORCE_HEADER 22
#define TRB_TYPE_CMD_NOOP 23
// events
#define TRB_TYPE_TRANSFER 32
#define TRB_TYPE_COMMAND_COMPLETION 33
#define TRB_TYPE_PORT_STATUS_CHANGE 34
#define TRB_TYPE_BANDWIDTH_REQUEST 35
#define TRB_TYPE_DOORBELL 36
#define TRB_TYPE_HOST_CONTROLLER 37
#define TRB_TYPE_DEVICE_NOTIFICATION 38
#define TRB_TYPE_MFINDEX_WRAP 39
// vendor
#define TRB_TYPE_NEC_COMMAND_COMPLETION 48
#define TRB_TYPE_NEC_GET_FIRMWARE_REV 49

#define TRB_3_CYCLE_BIT (1U << 0)
#define TRB_3_TC_BIT (1U << 1)
#define TRB_3_ENT_BIT (1U << 1)
#define TRB_3_ISP_BIT (1U << 2)
#define TRB_3_EVENT_DATA_BIT (1U << 2)
#define TRB_3_NSNOOP_BIT (1U << 3)
#define TRB_3_CHAIN_BIT (1U << 4)
#define TRB_3_IOC_BIT (1U << 5)
#define TRB_3_IDT_BIT (1U << 6)
#define TRB_3_BEI_BIT (1U << 9)
#define TRB_3_DCEP_BIT (1U << 9)
#define TRB_3_PRSV_BIT (1U << 9)
#define TRB_3_BSR_BIT (1U << 9)
#define TRB_3_TRT_MASK (3U << 16)
#define TRB_3_DIR_IN (1U << 16)
#define TRB_3_TRT_OUT (2U << 16)
#define TRB_3_TRT_IN (3U << 16)
#define TRB_3_SUSPEND_ENDPOINT_BIT (1U << 23)
#define TRB_3_ISO_SIA_BIT (1U << 31)

#define TRB_3_TBC(x) (((x)&0x3) << 7)
#define TRB_3_TBC_GET(x) (((x) >> 7) & 0x3)
#define TRB_3_TLBPC(x) (((x)&0xf) << 16)
#define TRB_3_TLBPC_GET(x) (((x) >> 16) & 0xf)
#define TRB_3_ENDPOINT(x) (((x)&0xf) << 16)
#define TRB_3_ENDPOINT_GET(x) (((x) >> 16) & 0xf)
#define TRB_3_FRID(x) (((x)&0x7ff) << 20)
#define TRB_3_FRID_GET(x) (((x) >> 20) & 0x7ff)
#define TRB_3_SLOT(x) (((x)&0xff) << 24)
#define TRB_3_SLOT_GET(x) (((x) >> 24) & 0xff)


#define XHCI_MAX_EVENTS (16 * 13)
#define XHCI_MAX_COMMANDS (16 * 1)
#define XHCI_MAX_SLOTS 255
#define XHCI_MAX_PORTS 127
#define XHCI_MAX_ENDPOINTS 32
// the spec says 1023, however this would cross the page boundary
#define XHCI_MAX_SCRATCHPADS 256
#define XHCI_MAX_DEVICES 128
#define XHCI_MAX_TRANSFERS 8


struct xhci_trb {
  uint64_t address;
  uint32_t status;
  uint32_t flags;
} __attribute__((__aligned__(4)));


// Section 6.5
struct xhci_erst_element {
  uint64_t rs_addr;
  uint32_t rs_size;
  uint32_t rsvdz;
} __attribute__((__aligned__(64)));


struct xhci_device_context_array {
  uint64_t baseAddress[XHCI_MAX_SLOTS];
  struct {
    uint64_t padding;
  } __attribute__((__aligned__(64)));
  uint64_t scratchpad[XHCI_MAX_SCRATCHPADS];
};


struct xhci_slot_ctx {
  uint32_t dwslot0;
  uint32_t dwslot1;
  uint32_t dwslot2;
  uint32_t dwslot3;
  uint32_t reserved[4];
};

#define SLOT_0_ROUTE(x) ((x)&0xFFFFF)
#define SLOT_0_ROUTE_GET(x) ((x)&0xFFFFF)
#define SLOT_0_SPEED(x) (((x)&0xF) << 20)
#define SLOT_0_SPEED_GET(x) (((x) >> 20) & 0xF)
#define SLOT_0_MTT_BIT (1U << 25)
#define SLOT_0_HUB_BIT (1U << 26)
#define SLOT_0_NUM_ENTRIES(x) (((x)&0x1F) << 27)
#define SLOT_0_NUM_ENTRIES_GET(x) (((x) >> 27) & 0x1F)

#define SLOT_1_MAX_EXIT_LATENCY(x) ((x)&0xFFFF)
#define SLOT_1_MAX_EXIT_LATENCY_GET(x) ((x)&0xFFFF)
#define SLOT_1_RH_PORT(x) (((x)&0xFF) << 16)
#define SLOT_1_RH_PORT_GET(x) (((x) >> 16) & 0xFF)
#define SLOT_1_NUM_PORTS(x) (((x)&0xFF) << 24)
#define SLOT_1_NUM_PORTS_GET(x) (((x) >> 24) & 0xFF)

#define SLOT_2_TT_HUB_SLOT(x) ((x)&0xFF)
#define SLOT_2_TT_HUB_SLOT_GET(x) ((x)&0xFF)
#define SLOT_2_PORT_NUM(x) (((x)&0xFF) << 8)
#define SLOT_2_PORT_NUM_GET(x) (((x) >> 8) & 0xFF)
#define SLOT_2_TT_TIME(x) (((x)&0x3) << 16)
#define SLOT_2_TT_TIME_GET(x) (((x) >> 16) & 0x3)
#define SLOT_2_IRQ_TARGET(x) (((x)&0x7F) << 22)
#define SLOT_2_IRQ_TARGET_GET(x) (((x) >> 22) & 0x7F)

#define SLOT_3_DEVICE_ADDRESS(x) ((x)&0xFF)
#define SLOT_3_DEVICE_ADDRESS_GET(x) ((x)&0xFF)
#define SLOT_3_SLOT_STATE(x) (((x)&0x1F) << 27)
#define SLOT_3_SLOT_STATE_GET(x) (((x) >> 27) & 0x1F)

#define HUB_TTT_GET(x) (((x) >> 5) & 0x3)

struct xhci_endpoint_ctx {
  uint32_t dwendpoint0;
  uint32_t dwendpoint1;
  uint64_t qwendpoint2;
  uint32_t dwendpoint4;
  uint32_t reserved[3];
};


#define ENDPOINT_0_STATE(x) ((x)&0x3)
#define ENDPOINT_0_STATE_GET(x) ((x)&0x3)
#define ENDPOINT_0_MULT(x) (((x)&0x3) << 8)
#define ENDPOINT_0_MULT_GET(x) (((x) >> 8) & 0x3)
#define ENDPOINT_0_MAXPSTREAMS(x) (((x)&0x1F) << 10)
#define ENDPOINT_0_MAXPSTREAMS_GET(x) (((x) >> 10) & 0x1F)
#define ENDPOINT_0_LSA_BIT (1U << 15)
#define ENDPOINT_0_INTERVAL(x) (((x)&0xFF) << 16)
#define ENDPOINT_0_INTERVAL_GET(x) (((x) >> 16) & 0xFF)

#define ENDPOINT_1_CERR(x) (((x)&0x3) << 1)
#define ENDPOINT_1_CERR_GET(x) (((x) >> 1) & 0x3)
#define ENDPOINT_1_EPTYPE(x) (((x)&0x7) << 3)
#define ENDPOINT_1_EPTYPE_GET(x) (((x) >> 3) & 0x7)
#define ENDPOINT_1_HID_BIT (1U << 7)
#define ENDPOINT_1_MAXBURST(x) (((x)&0xFF) << 8)
#define ENDPOINT_1_MAXBURST_GET(x) (((x) >> 8) & 0xFF)
#define ENDPOINT_1_MAXPACKETSIZE(x) (((x)&0xFFFF) << 16)
#define ENDPOINT_1_MAXPACKETSIZE_GET(x) (((x) >> 16) & 0xFFFF)

#define ENDPOINT_2_DCS_BIT (1U << 0)

#define ENDPOINT_4_AVGTRBLENGTH(x) ((x)&0xFFFF)
#define ENDPOINT_4_AVGTRBLENGTH_GET(x) ((x)&0xFFFF)
#define ENDPOINT_4_MAXESITPAYLOAD(x) (((x)&0xFFFF) << 16)
#define ENDPOINT_4_MAXESITPAYLOAD_GET(x) (((x) >> 16) & 0xFFFF)


struct xhci_regs {
  uint8_t caplength;
  uint8_t reserved;
  uint16_t hci_version;
  uint32_t hcs_params1;
  uint32_t hcs_params2;
  uint32_t hcs_params3;
  uint32_t hcc_params1;
  uint32_t db_off;  /* Doorbell offset */
  uint32_t rts_off; /* Runtime register space offset */
  uint32_t hcc_params2;
} __packed;

class xhci /* TODO: bus subsystem? */ {
 public:
  xhci(pci::device *dev);
  ~xhci(void);

  /* Don't write, please :) */
  bool initialized = false;

 private:
  spinlock lock;
  pci::device *dev = NULL;


  int irq;
  volatile struct xhci_regs *regs;
  uint32_t cap_register_offset;
  uint32_t op_register_offset;

  semaphore cmd_complete;
  semaphore finish_transfers;
  semaphore event;

  bool context_size_shift;

  uint32_t read_cap_reg32(uint32_t reg);
  void write_cap_reg32(uint32_t reg, uint32_t val);

  uint32_t read_op_reg32(uint32_t reg);
  void write_op_reg32(uint32_t reg, uint32_t val);

  bool controller_halt(void);
  bool controller_reset(void);

  bool wait_op_bits(uint32_t reg, uint32_t mask, uint32_t expected);
};
