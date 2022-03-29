#pragma once

#include <asm.h>
#include <x86/apic_flags.h>
#include <printf.h>
#include <idt.h>



#define ENABLE_APIC_DEBUG
// #define APIC_ULTRA_VERBOSE


#define APIC_LOG(...) PFXLOG(GRN "APIC", __VA_ARGS__)

#ifdef ENABLE_APIC_DEBUG
#define APIC_DEBUG APIC_LOG
#else
#define APIC_DEBUG()
#endif


#ifdef APIC_ULTRA_VERBOSE
#define APIC_UDEBUG APIC_LOG
#else
#define APIC_UDEBUG(...)
#endif

namespace x86 {

  enum ApicMode : int { Invalid = 0, Disabled, XApic, X2Apic };

  class Apic final {
   public:
    ApicMode mode;
    off_t base_addr;
    uint8_t version;
    unsigned id;
    uint64_t spur_int_cnt;
    uint64_t err_int_cnt;
    uint64_t bus_freq_hz;
    uint64_t ps_per_tick;
    uint64_t cycles_per_us;
    uint64_t cycles_per_tick;
    uint8_t timer_set;
    uint32_t current_ticks;  // timeout currently being computed
    uint64_t timer_count;
    int in_timer_interrupt;
    int in_kick_interrupt;

    // initialize this Apic for the calling core.
    void init();
    void dump();
    // setup the timer with a certain quantum in ms
    void timer_setup(uint32_t quantum_ms);
    void calibrate(void);

    // send a core (or all cores, if `core == -1`), an irq of 'vec'
    void ipi(int remote_id, int vec) {
      // APIC_DEBUG("Sending ipi %02x to %d\n", vec, remote_id);
      write_icr(remote_id, APIC_DEL_MODE_FIXED | (vec + T_IRQ0));
    }

    int set_mode(ApicMode mode);
    inline bool is_x2(void) const { return mode == ApicMode::X2Apic; }


    inline uint64_t ns_to_ticks(uint64_t ns) const { return ((ns * 1000ULL) / ps_per_tick); }
    inline uint64_t ns_to_cycles(uint64_t ns) const { return (ns * cycles_per_us) / 1000ULL; }
    inline uint64_t cycles_to_ns(uint64_t cycles) const { return ((cycles * 1000) / (cycles_per_us)); }

    void set_tickrate(uint32_t per_second);

    inline uint64_t ticks_per_second(void) { return bus_freq_hz; }


    int calibrate_timer_using_pit(int mode);


    inline void eoi(void) {
      // write the "End of Interrupt register"
      write(APIC_REG_EOI, 0);
    }

    inline void write_icr(uint32_t dest, uint32_t lo) {
      if (is_x2()) {
        uint64_t val = (((uint64_t)dest)) << 32 | lo;
        write64(APIC_REG_ICR, val);
      } else {
        write(APIC_REG_ICR2, dest << APIC_ICR2_DST_SHIFT);
        write(APIC_REG_ICR, lo);
      }
    }


    inline void msr_write(uint32_t msr, uint64_t data) {
      uint32_t lo = data;
      uint32_t hi = data >> 32;
      __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
    }
    inline uint64_t msr_read(uint32_t msr) const {
      uint32_t lo, hi;
      asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
      return ((uint64_t)hi << 32) | lo;
    }



    inline void write(uint32_t reg, uint32_t val) {
      if (is_x2()) {
        reg = X2APIC_MMIO_REG_OFFSET_TO_MSR(reg);
        APIC_UDEBUG("wr %08x <- %08x\n", reg, val);
        msr_write(reg, (uint64_t)val);
      } else {
        *((volatile uint32_t *)(base_addr + reg)) = val;
      }
    }

    inline uint32_t read(uint32_t reg) const {
      if (is_x2()) {
        reg = X2APIC_MMIO_REG_OFFSET_TO_MSR(reg);
        uint32_t val = (uint32_t)msr_read(reg);
        APIC_UDEBUG("rd %08x -> %08x\n", reg, val);
        return val;
      } else {
        return *((volatile uint32_t *)(base_addr + reg));
      }
    }




    /// these 64 bit read/write functions are only valid IFF the apic is and X2APIC
    inline void write64(uint32_t reg, uint64_t val) {
      if (is_x2()) {
        msr_write(X2APIC_MMIO_REG_OFFSET_TO_MSR(reg), val);
      } else {
        panic("apic_write_64 attemped while not using X2APIC\n");
      }
    }

    inline uint32_t read64(uint32_t reg) {
      if (is_x2()) {
        return msr_read(X2APIC_MMIO_REG_OFFSET_TO_MSR(reg));
      } else {
        panic("apic_read_64 attemped while not using X2APIC\n");
      }
      return 0;
    }
  };

}  // namespace x86
