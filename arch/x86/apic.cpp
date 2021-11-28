
// implementation for x86::Apic
#include <x86/apic.h>
#include <cpu.h>
#include <x86/msr.h>
#include <x86/cpuid.h>
#include <x86/smp.h>
#include <pit.h>
#include <sched.h>
#include <module.h>

#define IPI_IRQ (0xF3 - 32)

// how fast does the apic timer tick in
// the same time as a kernel tick
static uint32_t apic_ticks_per_second = 0;
static x86::Apic *bsp_apic = NULL;

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
 * (NOTE: taken from Nautilus, which took it from Kitten)
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



static void xcall_handler(int i, reg_t *tf, void *) {
  cpu::run_pending_xcalls();
  core().apic.eoi();
}

static void apic_tick_handler(int i, reg_t *tf, void *) {
  auto &cpu = cpu::current();
  uint64_t now = arch_read_timestamp();
  cpu.kstat.tsc_per_tick = now - cpu.kstat.last_tick_tsc;
  cpu.kstat.last_tick_tsc = now;
  cpu.kstat.ticks++;
  core().apic.eoi();
  if (core().in_sched) sched::handle_tick(cpu.kstat.ticks);
}




// Initialize the current CPU's APIC
void Apic::init(void) {
  if (bsp_apic == NULL) bsp_apic = this;

  uint32_t val;
  ApicMode curmode, maxmode;
  // sanity check.
  assert(&core().apic == this);

  curmode = get_mode();
  maxmode = get_max_mode();

#ifndef CONFIG_X2APIC
  if (maxmode == ApicMode::X2Apic) {
    APIC_DEBUG("The hardware supports X2APIC, but we have elected to use XAPIC instead\n");
    maxmode = ApicMode::XApic;
  }
#endif

  APIC_DEBUG("APIC's initial mode is %s\n", apic_modes[curmode]);

  if (int res = set_mode(maxmode); res != 0) {
    panic("x86::Apic::set_mode() returned %d\n", res);
  }

  if (mode != ApicMode::X2Apic) {
    /* Get the APIC base address, probably from the MSR */
    base_addr = apic_get_base_addr();
    /* idempotent when not compiled as HRT */
    this->base_addr = (off_t)p2v(base_addr);
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



  // clear the ESR (Error status register)
  write(APIC_REG_ESR, 0u);

  // Global Enable
  msr_write(APIC_BASE_MSR, msr_read(APIC_BASE_MSR) | APIC_GLOBAL_ENABLE);

  // assign spiv
  write(APIC_REG_SPIV, read(APIC_REG_SPIV) | APIC_SPUR_INT_VEC);

  // enable IER
  // write(APIC_REG_EXFC, read(APIC_REG_EXFR) | 0b1);
  // enable SEOI
  // write(APIC_REG_EXFC, read(APIC_REG_EXFR) | 0b10);

  // Turn it on.
  write(APIC_REG_SPIV, read(APIC_REG_SPIV) | APIC_SPIV_SW_ENABLE);



  // TODO:
  // mask 8259a interrupts (PIC)
  write(APIC_REG_LVT0, APIC_DEL_MODE_EXTINT | APIC_LVT_DISABLED);


  if (core().primary) {
    irq::install(50, apic_tick_handler, "Local APIC Preemption Tick");
    irq::install(IPI_IRQ, xcall_handler, "xcall handler");
  }
  // setup the timer with the correct quantum
  timer_setup(1000 / CONFIG_TICKS_PER_SECOND);


  dump();
}


void Apic::timer_setup(uint32_t quantum_ms) {
  uint32_t busfreq;
  uint32_t tmp;
  cpuid::ret_t ret;
  int x2apic, tscdeadline, arat;

  APIC_DEBUG("Setting up Local APIC timer for APIC 0x%x\n", this->id);

  cpuid::run(0x1, ret);

  x2apic = (ret.c >> 21) & 0x1;
  tscdeadline = (ret.c >> 24) & 0x1;
  cpuid::run(0x6, ret);
  arat = (ret.a >> 2) & 0x1;
  APIC_DEBUG("APIC timer has:  x2apic=%d tscdeadline=%d arat=%d\n", x2apic, tscdeadline, arat);

  // set the TiMer Divide CountR
  write(APIC_REG_TMDCR, APIC_TIMER_DIVCODE);

  if (apic_ticks_per_second == 0) {
    struct cpuid_busfreq_info freq;
    cpuid_busfreq(&freq);
    APIC_DEBUG("freq info: base: %uMHz,  max: %uMHz, bus: %uMHz\n", freq.base, freq.max, freq.bus);

    calibrate();
    apic_ticks_per_second = this->ticks_per_second();
  }

  set_tickrate(CONFIG_TICKS_PER_SECOND);

}




#define PIT_TIMER_IRQ 0

/* for configuring the PC speaker ports */
#define KB_CTRL_DATA_OUT 0x60
#define KB_CTRL_PORT_B 0x61

#define PIT_CHAN0_DATA 0x40
#define PIT_CHAN1_DATA 0x41
#define PIT_CHAN2_DATA 0x42
#define PIT_CMD_REG 0x43

/* for cmd reg */

#define PIT_CHAN_SEL_0 0x0
#define PIT_CHAN_SEL_1 0x1
#define PIT_CHAN_SEL_2 0x2

#define PIT_CHAN(x) ((x) << 6)

#define PIT_ACC_MODE_LATCH_CNT 0x0
#define PIT_ACC_MODE_JUST_LO 0x1
#define PIT_ACC_MODE_JUST_HI 0x2
#define PIT_ACC_MODE_BOTH 0x3

#define PIT_ACC_MODE(x) ((x) << 4)

#define PIT_MODE_TERMINAL_COUNT 0x0
#define PIT_MODE_ONESHOT 0x1
#define PIT_MODE_RATEGEN 0x2
#define PIT_MODE_SQUARE 0x3
#define PIT_MODE_SWSTROBE 0x4
#define PIT_MODE_HWSTROBE 0x5
#define PIT_MODE_RATEGEN2 0x6
#define PIT_MODE_SQUARE2 0x7

#define PIT_MODE(x) ((x) << 1)

#define PIT_BIT_MODE_BIN 0
#define PIT_BIT_MODE_BCD 1 /* 4-digit BCD */

#define PIT_RATE 1193182UL

#define MS 10
#define LATCH (PIT_RATE / (1000 / MS))
#define LOOPS 1000

static inline void io_delay(void) {
  const uint16_t DELAY_PORT = 0x80;
  asm volatile("outb %%al,%0" : : "dN"(DELAY_PORT));
}


// this is 10 ms (1/100)
#define TEST_TIME_SEC_RECIP 100
#define MAX_TRIES 100
int Apic::calibrate_timer_using_pit(int mode) {
  uint64_t start, end;
  uint16_t pit_count;
  uint32_t tries = 0;

try_once:

  // First determine the APIC's bus frequency by calibrating it
  // against a known clock (the PIT).   We do not know the base
  // rate of this APIC, but we do know the base rate of all PITs.
  // The PIT counts at about 1193180 Hz (~1.2 MHz)
  //

  pit_count = (1193180 / TEST_TIME_SEC_RECIP);

  if (mode == 1) {
    // In the way we are using the PIT, it will count one more than
    // the target count, hence the "- 1" in the following:
    pit_count--;
  }

  // Use APIC timer in one shot mode with the divider we will use
  // in normal execution.  We will count down from a large number
  // and do not expect interrupts because it should not hit zero.
  // timer is masked because we don't want an interrupt to occur at all
  // On KNL, masking also seems to have the side effect of reseting the count
  write(APIC_REG_LVTT, APIC_TIMER_ONESHOT | APIC_DEL_MODE_FIXED | APIC_TIMER_INT_VEC | APIC_TIMER_MASK);
  write(APIC_REG_TMDCR, APIC_TIMER_DIVCODE);


  // The following logic is based on the Intersil 8254 datasheet,
  // combined with the well-known logic for how the 8254 is used in
  // legacy support on PC-compatible platforms.
  //
  // The 8254 PIT appears in legacy land at ports
  // The 8254 PIT has 3 counters wired on a PC as follows:
  //
  //    0: IRQ 0 => not used in Nautilus
  //    1: DMA refresh => not used in any modern hardware
  //    2: Speaker/Output => we use this, as is common
  //
  // Counter 2 is gated by an output of the keyboard controller, and
  // its output is controlled via the same path.  Its output is
  // made available on an input of the keyboard controller.
  // These are visible at port 0x61 in legacy land.
  //
  // At this point, all interrupts are off and masked on the CPU.
  // Also, the 8254 is in reset state, which means it is undefined,
  // but also that it will not raise any output from any counter.
  //
  // An 8254 counter is trivial. First you write a mode for counting
  // on the counter to the 8254's command register (0x43).
  // Next you write a count to the port corresonding to the counter
  // (0x40-0x42) with the LSB first, then the MSB.  A gate input
  // determines if the counter runs.  The gates are controlled via
  // port 0x61.  In some modes, if the gate is high, the write of the MSB
  // will start the counter.   In others, you need to use the the gate
  // as an edge trigger to start it.
  //
  //
  // In mode 0 we will use the PIT (8254)'s counter 2 in mode 0 "Interrupt on
  // Terminal Count" and keep the gate high.  In this one-shot mode,
  // the LSB write to the initial count register will start the
  // counter.  The output will transition low before the LSB write,
  // and then high after the count is done.
  //
  // In mode 1 we will use the PIT (8254)'s counter 2 in mode 1 "Hardware
  // Retriggerable One-shot" and give the gate a positive edge.   In this
  // one-shot mode, it is the gate edge that causes it to start counting
  // The output will transition low after the gate edge and then high
  // after the count is done.

  uint8_t gate, detect, ctrl;

  // configure PIT channel 2 for terminal count in binary
  //
  // 10 11 000 0 (mode 0) or 10 11 001 0 (mode 1)

  ctrl = PIT_CHAN(PIT_CHAN_SEL_2) | PIT_ACC_MODE(PIT_ACC_MODE_BOTH);

  if (mode == 0) {
    ctrl |= PIT_MODE(PIT_MODE_TERMINAL_COUNT);
  } else {
    ctrl |= PIT_MODE(PIT_MODE_ONESHOT);
  }

  outb(PIT_CMD_REG, ctrl);



  // PIT  channel 2 is wired to the keyboard controller, which
  // allows to read when the PIT count hits zero.  Normally this path
  // is used to buzz the internal speaker (really!).
  // Port 0x61 bit 0 gates channel 2 counter
  //         mode 0: high to run
  //         mode 1: positive edge to run
  // Port 0x61 bit 1 gates channel 2 output to physical speaker (=1 to run
  // speaker) Port 0x61 bit 5 is channel 2 counter output (regardless of speaker
  // being driven)

  // Now enable it - we want gate on, speaker off

  gate = inb(KB_CTRL_PORT_B);

  gate &= 0xfc;  // channel 2 speaker output disabled

  if (mode == 0) {
    gate |= 0x1;  // channel 2 gate enabled, write of count will start counter
  } else {
    // gate disabled, write of gate will start counter
  }

  outb(KB_CTRL_PORT_B, gate);



  // PIT channel 2 counter is now waiting for a count to start/restart
  // we do this LSB then MSB

  outb(pit_count & 0xff, PIT_CHAN2_DATA);  // LSB

  io_delay();

  outb((uint8_t)(pit_count >> 8), PIT_CHAN2_DATA);  // MSB

  if (mode == 0) {
    // The PIT counter is now running
  } else {
    // pulse
    ctrl = inb(KB_CTRL_PORT_B);
    ctrl &= 0xfe;
    outb(ctrl, KB_CTRL_PORT_B);  // force gate low if it's not alread
    ctrl |= 0x1;
    outb(ctrl, KB_CTRL_PORT_B);  // force gate high to give positive edge
                                 // the PIT counter is now running
  }

  /// reset apic timer to our count down value
  write(APIC_REG_TMICT, 0xffffffff);


  // we need to be sure we are not racing with a 1->0 transition on the
  // output line.  Therefore we will wait for at least one clock interval at
  // 1.19.. Mhz which is 838 ns.   a port 0x80 read will take 1 us, so
  // we introduce at least 2 us here to be safe
  io_delay();
  io_delay();

  int count = 0;
  start = arch_read_timestamp();
  // we are now waiting for the PIT to finish
  // We are much faster than the PIT, so this loop should spin at least once
  while (1) {
    detect = inb(KB_CTRL_PORT_B);
    if ((detect & 0x20)) {
      break;
    }
    count++;
  }
  end = arch_read_timestamp();


  if (count == 0) {
    // this is a weird machine that does not get this mode right...
    APIC_DEBUG("PIT scan count of zero encountered, failing calibration for mode %d\n", mode);
    return -1;
  }


  // a known amount of real-time has now finished and we have
	// 1/TEST_TIME_SEC_RECIP seconds of real time in APIC timer ticks
  uint32_t apic_timer_ticks = 0xffffffff - read(APIC_REG_TMCCT) + 1;

  APIC_DEBUG(
      "One test period (1/%u sec) took %u APIC ticks, pit_count=%u, and %lu "
      "cycles\n",
      TEST_TIME_SEC_RECIP, apic_timer_ticks, (unsigned)pit_count, end - start);

  // occasionally, real hardware can provide surprise results here, probably due to an SMI.
  if (tries < MAX_TRIES && (end - start < 1000000 || apic_timer_ticks < 100)) {
    APIC_DEBUG("Test Period is impossible - trying again\n");
    tries++;
    goto try_once;
  }

  // us are used here to also keep precision for cycle->ns and ns->cycles conversions
  this->cycles_per_us = ((end - start) * TEST_TIME_SEC_RECIP) / 1000000;
  this->bus_freq_hz = APIC_TIMER_DIV * apic_timer_ticks * TEST_TIME_SEC_RECIP;
  this->ps_per_tick = (1000000000000ULL / this->bus_freq_hz) * APIC_TIMER_DIV;

	uint64_t mhz = ((this->cycles_per_us * 1000000) / 1000000) * APIC_TIMER_DIV;

  APIC_DEBUG("Detected APIC 0x%x bus frequency as %lu Hz\n", this->id, this->bus_freq_hz);
  APIC_DEBUG("Detected APIC 0x%x real time per tick as %lu ps\n", this->id, this->ps_per_tick);
  APIC_DEBUG("Detected APIC 0x%x as %lucyc/us (core at %lu.%03lu Ghz)\n", this->id, this->cycles_per_us, mhz / 1000, mhz % 1000);

  if (!this->bus_freq_hz || !this->ps_per_tick || !this->cycles_per_us) {
    APIC_DEBUG("Detected time calibration numbers cannot be zero....\n");
    return -1;
  }

  APIC_DEBUG("Succeeded in calibration with mode %d - spun for %d iterations\n", mode, count);
  return 0;
}


// TODO: move to manual oneshots
void Apic::set_tickrate(uint32_t per_second) {
  core().ticks_per_second = per_second;
  write(APIC_REG_TMDCR, APIC_TIMER_DIVCODE);

	auto ms = 1000 / per_second;
	auto ticks = realtime_to_ticks(ms * 1000000ULL);
	APIC_DEBUG("ticks: %llu\n", ticks);
  // set the current timer count
  this->write(APIC_REG_TMICT, ticks);

  // enable periodic ticks on irq 50
  write(APIC_REG_LVTT, APIC_TIMER_PERIODIC | (50 + T_IRQ0));
}


// will ruin the PIT, which might be a good thing :)
void Apic::calibrate(void) {
  if (!core().primary) {
    // clone core bsp, assuming it is already up
    // extern struct naut_info nautilus_info;
    this->bus_freq_hz = bsp_apic->bus_freq_hz;
    this->ps_per_tick = bsp_apic->ps_per_tick;
    this->cycles_per_us = bsp_apic->cycles_per_us;
    this->cycles_per_tick = bsp_apic->cycles_per_tick;
    APIC_DEBUG("AP APIC id=0x%x cloned BSP APIC's timer configuration\n", this->id);
    return;
  }


  APIC_DEBUG("APIC id=0x%x begining calibration\n", this->id);

  int mode;

  // We use PIT calibration, trying mode 0 first, which should work correctly
  // on all machines and on emulation (Phi, Qemu).
  if (mode = 0, calibrate_timer_using_pit(mode)) {
    // if we fail, we will try mode 1, which hopefully will work correctly on
    // most real hardware
    APIC_DEBUG("Failed calibration using PIT mode 0, retrying with mode 1\n");
    if (mode = 1, calibrate_timer_using_pit(mode)) {
      APIC_DEBUG(
          "Failed calibration PIT modes 0 and 1.... Time has no meaning "
          "here...\n");
      panic(
          "Failed calibration PIT modes 0 and 1.... Time has no meaning "
          "here...\n");
      return;
    }
  }
}

int Apic::set_mode(ApicMode newmode) {
  uint64_t val;
  ApicMode cur = get_mode();

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

  /*
auto exfr = read(APIC_REG_EXFR);
APIC_DEBUG("  EXFR: 0x%08x\n", exfr);
APIC_DEBUG("  ESR: 0x%08x (Error Status Reg, non-zero is bad)\n", read(APIC_REG_ESR));
APIC_DEBUG("  SVR: 0x%08x (Spurious vector=%d, %s, %s)\n", read(APIC_REG_SPIV), read(APIC_REG_SPIV) & APIC_SPIV_VEC_MASK,
(read(APIC_REG_SPIV) & APIC_SPIV_SW_ENABLE) ? "APIC IS ENABLED" : "APIC IS DISABLED",
(read(APIC_REG_SPIV) & APIC_SPIV_CORE_FOCUS) ? "Core Focusing Disabled" : "Core Focusing Enabled");
                  */


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
  APIC_DEBUG("      IRR pending:");
  for (int i = 0; i < 8; i++) {
    for (int b = 0; b < 32; b++) {
      int irq = b + (i * 8);
      auto irr = read(APIC_GET_IRR(i));
      int set = (irr >> b) & 1;

      if (set) printk(" %d", irq);
    }
  }
  printk("\n");
  APIC_DEBUG("          %08x %08x %08x %08x\n", read(APIC_GET_IRR(0)), read(APIC_GET_IRR(1)), read(APIC_GET_IRR(2)), read(APIC_GET_IRR(3)));
  APIC_DEBUG("          %08x %08x %08x %08x\n", read(APIC_GET_IRR(4)), read(APIC_GET_IRR(5)), read(APIC_GET_IRR(6)), read(APIC_GET_IRR(7)));




  APIC_DEBUG("      ISR pending:");
  for (int i = 0; i < 8; i++) {
    for (int b = 0; b < 32; b++) {
      int irq = b + (i * 8);
      auto irr = read(APIC_GET_ISR(i));
      int set = (irr >> b) & 1;

      if (set) printk(" %d", irq);
    }
  }
  printk("\n");
  APIC_DEBUG("          %08x %08x %08x %08x\n", read(APIC_GET_ISR(0)), read(APIC_GET_ISR(1)), read(APIC_GET_ISR(2)), read(APIC_GET_ISR(3)));
  APIC_DEBUG("          %08x %08x %08x %08x\n", read(APIC_GET_ISR(4)), read(APIC_GET_ISR(5)), read(APIC_GET_ISR(6)), read(APIC_GET_ISR(7)));



  /*
if (exfr & 0b1) {
APIC_DEBUG("      IER pending:");
for (int i = 0; i < 8; i++) {
for (int b = 0; b < 32; b++) {
  int irq = b + (i * 8);
  auto irr = read(APIC_GET_IER(i));
  int set = (irr >> b) & 1;

  if (set) printk(" %d", irq);
}
}

printk("\n");
APIC_DEBUG(
  "          %08x %08x %08x %08x\n", read(APIC_GET_IER(0)), read(APIC_GET_IER(1)), read(APIC_GET_IER(2)), read(APIC_GET_IER(3)));
APIC_DEBUG(
  "          %08x %08x %08x %08x\n", read(APIC_GET_IER(4)), read(APIC_GET_IER(5)), read(APIC_GET_IER(6)), read(APIC_GET_IER(7)));
}
  */
}




void arch_deliver_xcall(int id) { core().apic.ipi(id, IPI_IRQ); }

// only one core can dump at a time.
static spinlock apic_dump_lock;
static void apic_dump_request(void *_arg) {
  // :^)
  scoped_irqlock l(apic_dump_lock);
  core().apic.dump();
}



ksh_def("apic", "Dump the current core's APIC") {
  apic_dump_request(NULL);
  return 0;
}


ksh_def("apics", "Dump all cores' APIC") {
  // we have to use IPI to do this, as a core can only read it's own APIC
  // (at least with the implementation I have written :>)
  cpu::xcall(-1, apic_dump_request, NULL);
  return 0;
}
