#pragma once



namespace time {
	unsigned long long now_us(void);
	unsigned long long now_ms(void);

	// called by the timekeeper thread on tick
	void timekeep();

	// called by the RTC irq
	void set_second(unsigned long sec);
};
