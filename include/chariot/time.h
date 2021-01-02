#pragma once


#define MS_PER_SEC (1000LLU)
#define US_PER_SEC (MS_PER_SEC * 1000)
#define NS_PER_SEC (US_PER_SEC * 1000)

namespace time {
  unsigned long long now_ns(void);
  unsigned long long now_us(void);
  unsigned long long now_ms(void);

  unsigned long uptime(void);  // in seconds

  // called by the timekeeper thread on tick
  void timekeep();

  // called by the RTC irq
  void set_second(unsigned long sec);

  unsigned long cycles_to_ns(unsigned long cycles);

	void set_cps(unsigned long cps);

	/* this function is a callback to get the current NS (if the arch can handle it) */
	void set_high_accuracy_time_fn(unsigned long(*fn)(void));

	bool stabilized(void);
};  // namespace time
