#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <dev/RTC.h>
#include <time.h>

#define MS_PER_SEC (1000)
#define US_PER_SEC (MS_PER_SEC * 1000)
#define NS_PER_SEC (US_PER_SEC * 1000)




static unsigned long last_cycle = 0;
static uint64_t cycles_per_second = 0;
static volatile uint64_t current_second = 0;



bool time::stabilized(void) {
	return cycles_per_second != 0;
}

unsigned long time::cycles_to_ns(unsigned long cycles) {
  /* hmm */
  if (cycles_per_second == 0) return 0;
  return (cycles * NS_PER_SEC) / cycles_per_second;
}

unsigned long long time::now_ns() {
  auto cps = cycles_per_second;
  /* if we haven't be calibrated yet, guess based on the LAPIC tick rate */
  if (cps == 0) {
    auto &cpu = cpu::current();
    cps = cpu.kstat.tsc_per_tick * cpu.ticks_per_second;
  }

  uint64_t now_cycle = arch::read_timestamp();
  uint64_t delta = now_cycle - last_cycle;

  /*
   * this is a big hack. It accounts for variable IRQ timings
   * and avoids time travel in the case that a second takes
   * a different number of cycles.
   */
  if (delta > cps) delta = cps;

  uint64_t ns = (current_second * NS_PER_SEC);

  if (cycles_per_second != 0) {
    ns += time::cycles_to_ns(delta);
  }

  return ns;
}

unsigned long long time::now_us(void) {
  // TODO: this function can possibly return the same value multiple times?
  // I'm sure this is fine, but still.
  return now_ns() / 1000;  // (current_second * US_PER_SEC) + us_this_second;
}

unsigned long long time::now_ms(void) { return now_us() / 1000; }



// called every second by the RTC in x86
void time::timekeep(void) {
  auto now_second = dev::RTC::now();
  if (now_second != current_second) {
    unsigned long now_cycle = arch::read_timestamp();
    if (last_cycle != 0) {
      cycles_per_second = now_cycle - last_cycle;
    }
    last_cycle = now_cycle;
  }
  time::set_second(now_second);
}

void time::set_second(unsigned long sec) { current_second = sec; }
