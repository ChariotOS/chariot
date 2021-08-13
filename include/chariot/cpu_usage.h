#pragma once

// the user/kernel cpu usage structure definition



// how many ticks there have been since boot for each kind of task
struct chariot_core_usage {
  unsigned long idle_ticks;
  unsigned long user_ticks;
  unsigned long kernel_ticks;

  unsigned long ticks_per_second;
};