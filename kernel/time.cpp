#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <dev/RTC.h>
#include <time.h>

#define MS_PER_SEC (1000)
#define US_PER_SEC (MS_PER_SEC * 1000)
#define NS_PER_SEC (US_PER_SEC * 1000)




static unsigned long last_cycle = 0;

#define CYCLE_RECORD_COUNT 100
unsigned long cycle_record[CYCLE_RECORD_COUNT];
static off_t index = 0;
static unsigned long long avg_cycles_per_tick = 0;
static unsigned long current_second = 0;
static unsigned long us_this_second = 0;



unsigned long long time::now_ns() {
	/*
  unsigned long cycle_drift = arch::read_timestamp() - last_cycle;
  unsigned long tps = cpu::current().ticks_per_second;
  unsigned long long ns_per_tick = (NS_PER_SEC) / tps;
  long long avg_cycles_per_us = avg_cycles_per_tick / ns_per_tick;
  long long us_drift = cycle_drift / avg_cycles_per_us;
	*/
  return (current_second * NS_PER_SEC) + (us_this_second * 1000);// + (us_drift);
}

unsigned long long time::now_us(void) {
  // TODO: this function can possibly return the same value multiple times?
  // I'm sure this is fine, but still.
  return now_ns() / 1000;  // (current_second * US_PER_SEC) + us_this_second;
}

unsigned long long time::now_ms(void) { return now_us() / 1000; }


void time::timekeep(void) {
  unsigned long now_cycle = arch::read_timestamp();
  unsigned long tps = cpu::current().ticks_per_second;
  unsigned long delta = US_PER_SEC / tps;

  us_this_second += delta;
  if (last_cycle != 0) {
    if (index == CYCLE_RECORD_COUNT) {
      // average everything up
      index = 0;

      unsigned long long total_cycles = 0;
      for (int i = 0; i < CYCLE_RECORD_COUNT; i++) total_cycles += cycle_record[i];
      avg_cycles_per_tick = (total_cycles / CYCLE_RECORD_COUNT);
    }

    cycle_record[index++] = now_cycle - last_cycle;
  }

  unsigned long long us_per_tick = (US_PER_SEC) / tps;
  unsigned long long cycles_this_tick = now_cycle - last_cycle;
  if (avg_cycles_per_tick == 0) avg_cycles_per_tick = cycles_this_tick;
  last_cycle = now_cycle;

  long long avg_cycles_per_us = avg_cycles_per_tick / us_per_tick;
  long long cycle_drift = avg_cycles_per_tick - cycles_this_tick;
  long long us_drift = cycle_drift / avg_cycles_per_us;

  if (us_drift > 0) {
    us_this_second += us_drift;
  }

  if (unlikely(us_this_second >= US_PER_SEC)) {
    time::set_second(dev::RTC::now());
  }
}

void time::set_second(unsigned long sec) {
  current_second = sec;
  us_this_second = 0;
}
