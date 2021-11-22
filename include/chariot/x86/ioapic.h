#pragma once

#include <types.h>


#define IOAPIC_DEFAULT_BASE 0xfec00000

/* IOAPIC MMIO registers */
#define IOREGSEL_REG 0x00  // I/O Register Select (index)
#define IOWIN_REG 0x10     // I/O Window (data)


/* Internal register offsets */
#define IOAPICID_REG 0x00  // IOAPIC ID
#define IOAPIC_GET_ID(x) (((x) >> 24) & 0xfu)
#define IOAPICVER_REG 0x01  // IOAPIC Version
#define IOAPIC_GET_VER(x) ((x)&0xffu)
#define IOAPIC_GET_MAX_RED(x) (((x) >> 16) & 0xffu)
#define IOAPICARB_REG 0x02  // IOAPIC Arbitration ID
#define IOAPIC_GET_ARBID(x) (((x) >> 24) & 0xfu)
#define IOREDTBL 0x10  // Redirection Table (Entries 0-23) (64 bits each)

/* used for accessing IOREDTBL entries */
#define IOAPIC_IRQ_ENTRY_LO(n) (IOREDTBL + 2 * (n))
#define IOAPIC_IRQ_ENTRY_HI(n) (IOREDTBL + (2 * (n)) + 1)

#define IORED_VEC_MASK 0xffff
#define IORED_DEL_MODE_MASK (0x7 << 8)

#define IORED_GET_DEST(x) (((x) >> 56) & 0xffu)
#define IORED_GET_MASK(x) (((x) >> 16) & 1)
#define IORED_GET_TRIG(x) (((x) >> 15) & 1)
#define IORED_GET_RIRR(x) (((x) >> 14) & 1)
#define IORED_GET_POL(x) (((x) >> 13) & 1)
#define IORED_GET_DST_MODE(x) (((x) >> 11) & 1)
#define IORED_GET_DEL_MODE(x) (((x) >> 8) & 0x7)
#define IORED_GET_VEC(x) ((x)&0xffu)


#define IORED_DEST_MODE (1 << 11)
#define IORED_DELIVS (1 << 12)
#define IORED_INTPOL (1 << 13)
#define IORED_TRIGGER_MODE (1 << 15)
#define IORED_INT_MASK (1 << 16)

#define DEL_MODE_SHIFT 8
#define DEST_MODE_SHIFT 11
#define DELIVS_SHIFT 12
#define INTPOL_SHIFT 13
#define TRIG_MODE_SHIFT 15
#define INT_MASK_SHIFT 16

#define IORED_DST_MASK_PHY 0x0f000000
#define IORED_DST_MASK_LOG 0xff000000

#define DELMODE_FIXED 0x0
#define DELMODE_LOWEST 0x1
#define DELMODE_SMI 0x2
#define DELMODE_RSVD 0x3
#define DELMODE_NMI 0x4
#define DELMODE_INIT 0x5
#define DELMODE_RSVD1 0x6
#define DELMODE_EXTINT 0x7


#define PIN_POLARITY_HI 0
#define PIN_POLARITY_LO 1

#define TRIGGER_MODE_EDGE 0
#define TRIGGER_MODE_LEVEL 1

#define IOAPIC_MASK_IRQ (1 << 16)


namespace x86 {


  /*
struct IORedEntry {
// are we enabled or masked?
uint8_t enabled;

// what we got from the MP Tables
struct nk_int_entry* boot_info;

// the irq number that we assign
uint8_t actual_irq;
};
*/

  // there could be many...
	// Not one per core...
	// Likely one per socket...
  class IOApic final {
   public:
    uint8_t id;
    uint8_t version;
    uint8_t usable;
    addr_t base;

    void init(void);
  };
}  // namespace x86
