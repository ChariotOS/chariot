#include <arch.h>
#include <asm.h>
#include <cpu.h>
#ifdef CONFIG_X86
#include <dev/RTC.h>
#endif
#include <time.h>



static unsigned long last_cycle = 0;
static volatile uint64_t cycles_per_second = 0;
static volatile uint64_t current_second = 0;


void time::set_cps(unsigned long cps) {
	cycles_per_second = cps;
}

bool time::stabilized(void) { return cycles_per_second != 0; }


static unsigned long (*g_high_acc)(void) = NULL;
void time::set_high_accuracy_time_fn(unsigned long (*fn)(void)) {
	g_high_acc = fn;
}

unsigned long time::cycles_to_ns(unsigned long cycles) {
  /* hmm */
  if (cycles_per_second == 0) return 0;
  return (cycles * NS_PER_SEC) / cycles_per_second;
}

unsigned long long time::now_ns() {
	/* if the arch gives us this func, use it :) */
	if (g_high_acc != NULL) {
		return g_high_acc();
	}

  /* If the time is not stabilized yet, wait for it. */
  if (false && unlikely(!time::stabilized())) {
    printk(KERN_WARN "The time has not been stabilized before access. Waiting for stabilization...\n");
    while (!time::stabilized()) {
			arch_relax();
	}
    printk(KERN_WARN "Time stabilized.\n");
  }

  auto cps = cycles_per_second;
  /* if we haven't be calibrated yet, guess based on the LAPIC tick rate */
  if (cps == 0) {
    auto &cpu = cpu::current();
    cps = cpu.kstat.tsc_per_tick * cpu.ticks_per_second;
  }

  uint64_t now_cycle = arch_read_timestamp();
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
  return now_ns() / 1000;  // (current_second * US_PER_SEC) + us_this_second;
}

unsigned long long time::now_ms(void) { return now_us() / 1000; }



// called every second by the RTC in x86
void time::timekeep(void) {

#ifdef CONFIG_X86
  auto now_second = dev::RTC::now();
#else
  auto now_second = arch_seconds_since_boot();
#endif

  if (now_second != current_second) {
    unsigned long now_cycle = arch_read_timestamp();
    if (last_cycle != 0) {
      cycles_per_second = now_cycle - last_cycle;
    }
    last_cycle = now_cycle;
  }
  time::set_second(now_second);
}

void time::set_second(unsigned long sec) { current_second = sec; }
