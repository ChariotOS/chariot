#pragma once

#include <lock.h>
#include <single_list.h>
#include <types.h>

struct waitqueue_elem {
  void *priv;

  // arbitrary value, typically byte count
  u32 waiting_on = 0;
  u32 flags;
  struct task *waiter;

};

/**
 * implemented in sched.cpp
 */
class waitqueue {
 public:
  waitqueue(const char * = "generic_waitqueue");

  void wait(u32 on = 0);
  void notify();


  bool should_notify(u32 val);

 private:
  // navail is the number of unhandled notifications
  int navail = 0;
  single_list<waitqueue_elem> elems;
  const char *name;
  mutex_lock lock;
};
