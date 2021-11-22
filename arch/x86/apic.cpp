
// implementation for x86::Apic
#include <x86/apic.h>
#include <cpu.h>
#include <x86/msr.h>
#include <x86/cpuid.h>




using namespace x86;

static const char *apic_err_codes[8] = {"[Send Checksum Error]", "[Receive Checksum Error]", "[Send Accept Error]",
    "[Receive Accept Error]", "[Redirectable IPI]", "[Send Illegal Vector]", "[Receive Illegal Vector]", "[Illegal Register Address]"};

static const char *apic_modes[4] = {
    "Invalid",
    "Disabled",
    "XAPIC",
    "X2APIC",
};


static ApicMode get_mode() {
  uint64_t val;
  uint64_t EN, EXTD;

  val = msr_read(APIC_BASE_MSR);
  EN = (val >> 11) & 0x1;
  EXTD = (val >> 10) & 0x1;

  if (!EN && !EXTD) {
    return x86::ApicMode::Disabled;
  }

  if (EN && !EXTD) {
    return x86::ApicMode::XApic;
  }

  if (EN && EXTD) {
    return ApicMode::X2Apic;
  }

  return ApicMode::Invalid;
}


static ApicMode get_max_mode() {
  cpuid::ret_t c;
  cpuid::run(0x1, c);
  if ((c.c >> 21) & 0x1) {
    APIC_DEBUG("APIC with X2APIC support\n");
    return ApicMode::X2Apic;
  }
  return ApicMode::XApic;  // it must at least do this...
}



static off_t apic_get_base_addr(void) {
  uint64_t data;
  data = msr_read(APIC_BASE_MSR);

  // we're assuming PAE is on
  return (addr_t)(data & APIC_BASE_ADDR_MASK);
}




/**
 * Converts an entry in a local APIC's Local Vector Table to a
 * human-readable string.
 * (NOTE: taken from Kitten)
 */
static char *lvt_stringify(uint32_t entry, char *buf) {
  uint32_t delivery_mode = entry & 0x700;

  if (delivery_mode == APIC_DEL_MODE_FIXED) {
    sprintk(buf, "FIXED -> IDT VECTOR %u", entry & APIC_LVT_VEC_MASK);
  } else if (delivery_mode == APIC_DEL_MODE_NMI) {
    sprintk(buf, "NMI   -> IDT VECTOR 2");
  } else if (delivery_mode == APIC_DEL_MODE_EXTINT) {
    sprintk(buf, "ExtINT, hooked to old 8259A PIC");
  } else {
    sprintk(buf, "UNKNOWN");
  }

  if (entry & APIC_LVT_DISABLED) strcat(buf, ", MASKED");

  return buf;
}



// Initialize the current CPU's APIC
void Apic::init(void) {
  uint32_t val;
  ApicMode curmode, maxmode;
  // sanity check.
  assert(&core().apic == this);

  curmode = get_mode();
  maxmode = get_max_mode();

  APIC_DEBUG("APIC's initial mode is %s\n", apic_modes[curmode]);



  if (int res = set_mode(maxmode); res != 0) {
    panic("x86::Apic::set_mode() returned %d\n", res);
  }

  if (mode != ApicMode::X2Apic) {
    base_addr = apic_get_base_addr();

    /* idempotent when not compiled as HRT */
    this->base_addr = (off_t)p2v(base_addr);

#if 0
		// if the core is the bootstrap processor
    if (core().primary) {
      // map in the lapic as uncacheable
      if (nk_map_page_nocache(apic->base_addr, PTE_PRESENT_BIT | PTE_WRITABLE_BIT, PS_4K) == -1) {
        panic("Could not map APIC\n");
      }
    }
#endif
  }


  APIC_DEBUG("base address: %p. %d\n", base_addr, is_x2());

  this->version = APIC_VERSION(read(APIC_REG_LVR));
  this->id = is_x2() ? (this->read(APIC_REG_ID)) : ((this->read(APIC_REG_ID) >> APIC_ID_SHIFT) & 0xff);
  // update the core's id with the APIC's id
  core().id = this->id;

  if (this->version < 0x10 || this->version > 0x15) {
    panic("Unsupported APIC version (0x%1x)\n", (unsigned)this->version);
  }


  if (is_x2()) {
    // LDR is not writeable in X2APIC mode
    // we just have to go with what it is
    // see note in apic.h about how to derive
    // the logical id from the physical id
    val = read(APIC_REG_LDR);
    APIC_DEBUG("X2APIC LDR=0x%x (cluster 0x%x, logical id 0x%x)\n", val, APIC_LDR_X2APIC_CLUSTER(val), APIC_LDR_X2APIC_LOGID(val));

  } else {
    val = read(APIC_REG_LDR) & ~APIC_LDR_MASK;
    // flat group 1 is for watchdog NMIs.
    // At least one bit must be set in a group
    val |= SET_APIC_LOGICAL_ID(1);
    write(APIC_REG_LDR, val);
    write(APIC_REG_DFR, APIC_DFR_FLAT);
  }



  // accept all interrupts
  write(APIC_REG_TPR, read(APIC_REG_TPR) & 0xffffff00);


  // disable imter interrupts initially
  write(APIC_REG_LVTT, APIC_DEL_MODE_FIXED | APIC_LVT_DISABLED);
  // disable perf cntr interrupts
  write(APIC_REG_LVTPC, APIC_DEL_MODE_FIXED | APIC_LVT_DISABLED | APIC_PC_INT_VEC);

  // disable thermal interrupts
  write(APIC_REG_LVTTHMR, APIC_DEL_MODE_FIXED | APIC_LVT_DISABLED | APIC_THRML_INT_VEC);
  // mask 8259a interrupts (PIC)
  write(APIC_REG_LVT0, APIC_DEL_MODE_EXTINT | APIC_LVT_DISABLED);



  // clear the ESR (Error status register)
  write(APIC_REG_ESR, 0u);

  // Global Enable
  msr_write(APIC_BASE_MSR, msr_read(APIC_BASE_MSR) | APIC_GLOBAL_ENABLE);


  if (core().primary) {
    // TODO: initialize APIC-based interrupt handlers here, as all cores
    //       share the same IDT, we only need to set these up once.
  }


  // assign spiv
  write(APIC_REG_SPIV, read(APIC_REG_SPIV) | APIC_SPUR_INT_VEC);

  // Turn it on.
  write(APIC_REG_SPIV, read(APIC_REG_SPIV) | APIC_SPIV_SW_ENABLE);

  dump();
}


int Apic::set_mode(ApicMode newmode) {
  uint64_t val;
  ApicMode cur = get_mode();

  /*
if (newmode == cur) {
this->mode = newmode;
return 0;
}
  */

  if (newmode == ApicMode::Invalid) {
    return -1;
  }


  APIC_DEBUG("Setting APIC mode to maximum available mode (%s)\n", apic_modes[newmode]);

  // first go back to disabled
  val = msr_read(APIC_BASE_MSR);
  val &= ~(0x3 << 10);
  msr_write(APIC_BASE_MSR, val);
  // now go to the relevant mode progressing "upwards"
  if (newmode != ApicMode::Disabled) {
    // switch to XAPIC first
    APIC_DEBUG("Switching up to XAPIC\n");
    val |= 0x2 << 10;
    msr_write(APIC_BASE_MSR, val);
    if (newmode == ApicMode::X2Apic) {
      APIC_DEBUG("Switching up to X2APIC\n");
      // now to X2APIC if requested
      val |= 0x3 << 10;
      msr_write(APIC_BASE_MSR, val);
    }
  }
  this->mode = newmode;
  return 0;
}



void Apic::dump(void) {
  char buf[128];
  auto mode = get_mode();

  APIC_DEBUG("DUMP (LOGICAL CPU #%u):\n", core_id());

  APIC_DEBUG("  ID:  0x%08x (id=%d) (mode=%s)\n", read(APIC_REG_ID), !is_x2() ? APIC_GET_ID(read(APIC_REG_ID)) : read(APIC_REG_ID),
      apic_modes[mode]);

  APIC_DEBUG(
      "  VER: 0x%08x (version=0x%x, max_lvt=%d)\n", read(APIC_REG_LVR), APIC_LVR_VER(read(APIC_REG_LVR)), APIC_LVR_MAX(read(APIC_REG_LVR)));

  if (!is_x2()) {
    APIC_DEBUG("  BASE ADDR: %p\n", this->base_addr);
  }

  APIC_DEBUG("  ESR: 0x%08x (Error Status Reg, non-zero is bad)\n", read(APIC_REG_ESR));
  APIC_DEBUG("  SVR: 0x%08x (Spurious vector=%d, %s, %s)\n", read(APIC_REG_SPIV), read(APIC_REG_SPIV) & APIC_SPIV_VEC_MASK,
      (read(APIC_REG_SPIV) & APIC_SPIV_SW_ENABLE) ? "APIC IS ENABLED" : "APIC IS DISABLED",
      (read(APIC_REG_SPIV) & APIC_SPIV_CORE_FOCUS) ? "Core Focusing Disabled" : "Core Focusing Enabled");


  /*
   * Local Vector Table
   */
  APIC_DEBUG("  Local Vector Table Entries:\n");
  const char *timer_mode;
  if (read(APIC_REG_LVTT) & APIC_TIMER_PERIODIC) {
    timer_mode = "Periodic";
  } else if (read(APIC_REG_LVTT) & APIC_TIMER_TSCDLINE) {
    timer_mode = "TSC-Deadline";
  } else {
    timer_mode = "One-shot";
  }


  APIC_DEBUG("      LVT[0] Timer:     0x%08x (mode=%s, %s)\n", read(APIC_REG_LVTT), timer_mode, lvt_stringify(read(APIC_REG_LVTT), buf));
  APIC_DEBUG("      LVT[1] Thermal:   0x%08x (%s)\n", read(APIC_REG_LVTTHMR), lvt_stringify(read(APIC_REG_LVTTHMR), buf));
  APIC_DEBUG("      LVT[2] Perf Cnt:  0x%08x (%s)\n", read(APIC_REG_LVTPC), lvt_stringify(read(APIC_REG_LVTPC), buf));
  APIC_DEBUG("      LVT[3] LINT0 Pin: 0x%08x (%s)\n", read(APIC_REG_LVT0), lvt_stringify(read(APIC_REG_LVT0), buf));
  APIC_DEBUG("      LVT[4] LINT1 Pin: 0x%08x (%s)\n", read(APIC_REG_LVT1), lvt_stringify(read(APIC_REG_LVT1), buf));
  APIC_DEBUG("      LVT[5] Error:     0x%08x (%s)\n", read(APIC_REG_LVTERR), lvt_stringify(read(APIC_REG_LVTERR), buf));


  /*
   * APIC timer configuration registers
   */
  APIC_DEBUG("  Local APIC Timer:\n");
  APIC_DEBUG("      DCR (Divide Config Reg): 0x%08x\n", read(APIC_REG_TMDCR));
  APIC_DEBUG("      ICT (Initial Count Reg): 0x%08x\n", read(APIC_REG_TMICT));
  APIC_DEBUG("      CCT (Current Count Reg): 0x%08x\n", read(APIC_REG_TMCCT));


  /*
   * Logical APIC addressing mode registers
   */
  APIC_DEBUG("  Logical Addressing Mode Information:\n");
  if (!is_x2()) {
    APIC_DEBUG("      LDR (Logical Dest Reg):  0x%08x (id=%d)\n", read(APIC_REG_LDR), GET_APIC_LOGICAL_ID(read(APIC_REG_LDR)));
  } else {
    APIC_DEBUG("      LDR (Logical Dest Reg):  0x%08x (cluster=%d, id=%d)\n", read(APIC_REG_LDR),
        APIC_LDR_X2APIC_CLUSTER(read(APIC_REG_LDR)), APIC_LDR_X2APIC_LOGID(read(APIC_REG_LDR)));
  }
  if (!is_x2()) {
    APIC_DEBUG(
        "      DFR (Dest Format Reg):   0x%08x (%s)\n", read(APIC_REG_DFR), (read(APIC_REG_DFR) == APIC_DFR_FLAT) ? "FLAT" : "CLUSTER");
  }


  /*
   * Task/processor/arbitration priority registers
   */
  APIC_DEBUG("  Task/Processor/Arbitration Priorities:\n");
  APIC_DEBUG("      TPR (Task Priority Reg):        0x%08x\n", read(APIC_REG_TPR));
  APIC_DEBUG("      PPR (Processor Priority Reg):   0x%08x\n",
#ifndef NAUT_CONFIG_GEM5  // unimplemented in GEM5 - WTF?
      read(APIC_REG_PPR)
#else
      0
#endif
  );
  if (!is_x2()) {
    // reserved in X2APIC
    APIC_DEBUG("      APR (Arbitration Priority Reg): 0x%08x\n", read(APIC_REG_APR));
  }


  /*
   * ISR/IRR
   */
  APIC_DEBUG("  IRR/ISR:\n");
  APIC_DEBUG("      IRR %08x %08x %08x %08x\n", read(APIC_GET_IRR(0)), read(APIC_GET_IRR(1)), read(APIC_GET_IRR(2)), read(APIC_GET_IRR(3)));
  APIC_DEBUG("          %08x %08x %08x %08x\n", read(APIC_GET_IRR(4)), read(APIC_GET_IRR(5)), read(APIC_GET_IRR(6)), read(APIC_GET_IRR(7)));


  APIC_DEBUG("      IRR %08x %08x %08x %08x\n", read(APIC_GET_ISR(0)), read(APIC_GET_ISR(1)), read(APIC_GET_ISR(2)), read(APIC_GET_ISR(3)));
  APIC_DEBUG("          %08x %08x %08x %08x\n", read(APIC_GET_ISR(4)), read(APIC_GET_ISR(5)), read(APIC_GET_ISR(6)), read(APIC_GET_ISR(7)));

}
