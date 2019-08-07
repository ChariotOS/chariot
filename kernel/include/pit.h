#ifndef __PIT_H__
#define __PIT_H__

#include <types.h>

// various functions to enable, disable, and configure the PIT
void init_pit(void);

// takes number of interrupts in a second
void set_pit_freq(u16 per_sec);



#endif
