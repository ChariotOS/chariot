#pragma once

#include <lock.h>
#include <single_list.h>
#include <types.h>

struct waitqueue_elem {
  void *priv;
  u32 flags;
  struct task *waiter;

};

/**
 * implemented in sched.cpp
 */
class waitqueue {
 public:
  waitqueue(const char *);

  void wait();
  void notify();

 private:
  // navail is the number of unhandled notifications
  int navail = 0;
  single_list<waitqueue_elem> elems;
  const char *name;
  mutex_lock lock;
};
