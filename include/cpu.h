#pragma once

#include <types.h>

struct cpu_t {
  void *local;
  int ncli = 0;
  size_t ticks = 0;
  size_t intena = 0;
  u32 speed_khz;
};

namespace cpu {

// return the current cpu struct
cpu_t &current(void);

// setup CPU segment descriptors, run once per cpu
void seginit(void);

void calc_speed_khz(void);

// pushcli - disable interrupts and incrementing the depth if interrupt
// disable-ness (?)
void pushcli();
// popcli - pop the depth of disabled interrupts, and if it reaches zero, enable
// interrupts again
void popcli();

// plop at the top of a scope and interrupts will be disabled within that scope
class scoped_cli {
 public:
  inline scoped_cli() { pushcli(); }
  inline ~scoped_cli() { popcli(); }
};


}  // namespace cpu
