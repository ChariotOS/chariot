#ifndef __PIT_H__
#define __PIT_H__

#include <types.h>

// various functions to enable, disable, and configure the PIT
void init_pit(void);

// takes number of interrupts in a second
void set_pit_freq(u16 per_sec);

void pic_enable(uint8_t irq);
void pic_disable(uint8_t irq);
void pic_ack(uint8_t irq);


namespace pit {
  void dumb_sleep(unsigned ms);
}


#endif
