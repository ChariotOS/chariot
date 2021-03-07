#pragma once

#include <sched.h>

namespace sched::core {


  /*
   * Chariot has a "generic scheduler" interface which can be "implemented"
   * by different scheduler implementations.
   *
   * One of these schedulers are created at a time, and describes the inner
   * workings of the scheduler. This is done this way in order to allow swappable
   * schedulers in order to test different algorithms.
   *
   * This core interface is not meant to be constructed, and instead is a baseclass
   * for other implementations.
   */
  struct scheduler {
    // Add a task to this scheduler to be considered at the next choice call
    virtual int add_task(struct thread *) = 0;
    virtual int remove_task(struct thread *t) = 0;
  };
};  // namespace sched::core
