#include <time.h>
#include <asm.h>
#include <dev/RTC.h>
#include <cpu.h>
#include <arch.h>

#define US_PER_SEC (1000 * 1000)

static unsigned long current_second = 0;
static unsigned long us_this_second = 0;

unsigned long long time::now_us(void) {
	// TODO: this function can possibly return the same value multiple times?
	// I'm sure this is fine, but still.
	return (current_second * US_PER_SEC) + us_this_second; //  arch::us_this_second();
}

unsigned long long time::now_ms(void) {
	return now_us() / 1000;
}


void time::timekeep(void) {
	unsigned long tps = cpu::current().ticks_per_second;
	unsigned long delta = US_PER_SEC / tps;

	us_this_second += delta;

	if (unlikely(us_this_second >= US_PER_SEC)) {
		time::set_second(dev::RTC::now());
	}
}

void time::set_second(unsigned long sec) {
	current_second = sec;
	us_this_second = 0;
}
