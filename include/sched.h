// A single header file for all things task and scheduler related

#pragma once

#include <func.h>


using pid_t = int;
using gid_t = int;

// forward declare
class task;

namespace sched {

  bool init(void);

  bool enabled();

#define PRIORITY_IDLE 1
#define PRIOIRTY_LOW  10
#define PRIORITY_NORMAL 25
#define PRIORITY_HIGH   100

  struct create_opts {
    int timeslice = 1;
    int priority = PRIORITY_NORMAL;
  };

  pid_t spawn_kernel_task(const char *name, void (*e)(), create_opts opts = {});


  void yield(void);


  void block();

  // does not return
  void run(void);


  void handle_tick(u64 tick);

  // force the process to exit, (yield with different state)
  void exit();


  void play_tone(int frq, int dur);

  // make a 440hz beep for 100ms
  void beep();
}
