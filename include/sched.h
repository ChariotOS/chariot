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


  pid_t spawn_kernel_task(const char *name, void (*e)());


  void yield(void);

  // does not return
  void run(void);


  void handle_tick(u64 tick);

  // force the process to exit, (yield with different state)
  void exit();
}
