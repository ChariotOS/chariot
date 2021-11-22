#pragma once

#include <asm.h>
#include <x86/apic_flags.h>
#include <printk.h>



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

    int set_mode(ApicMode mode);
    inline bool is_x2(void) const { return mode == ApicMode::X2Apic; }


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
    inline void apic_write64(uint32_t reg, uint64_t val) {
      if (is_x2()) {
        msr_write(X2APIC_MMIO_REG_OFFSET_TO_MSR(reg), val);
      } else {
        panic("apic_write_64 attemped while not using X2APIC\n");
      }
    }

    inline uint32_t apic_read64(uint32_t reg) {
      if (is_x2()) {
        return msr_read(X2APIC_MMIO_REG_OFFSET_TO_MSR(reg));
      } else {
        panic("apic_read_64 attemped while not using X2APIC\n");
      }
			return 0;
    }
  };

}  // namespace x86
