#pragma once

#include <lock.h>
#include <ptr.h>
#include <single_list.h>
#include <types.h>

#define WAIT_NOINT 1

#define NOTIFY_RUDE (1 << 0)
#define NOTIFY_URGENT (1 << 1)


namespace wait {

  class queue;

  struct waiter : public refcounted<waiter> {
    inline waiter(wait::queue &wq) : wq(wq) {}

    virtual ~waiter(void) {}

    // returns if the notify was accepted
    virtual bool notify(int flags) = 0;
    //
    virtual void start() = 0;

    void interrupt(void);


    size_t waiting_on = 0;
    int flags = 0;
		wait::queue &wq;
		wait::waiter *prev, *next;
  };


  class queue {
   public:
    // wait on the queue, interruptable. Returns true if it was not interrupted
    bool WARN_UNUSED wait(u32 on = 0, ref<waiter> wtr = nullptr);

    // wait, but not interruptable
    void wait_noint(u32 on = 0, ref<waiter> wtr = nullptr);
    void notify(int flags = 0);

    void notify_all(int flags = 0);

    bool should_notify(u32 val);


    void interrupt(waiter *);
    bool do_wait(u32 on, int flags, ref<waiter> wtr);
    // navail is the number of unhandled notifications
    int navail = 0;

    spinlock lock;

    // next to pop on notify
    ref<waiter> front = nullptr;
    // where to put new tasks
    ref<waiter> back = nullptr;
  };

};  // namespace wait

