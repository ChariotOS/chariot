#include <cpu.h>
#include <wait.h>

/* the default "waiter" type */
class threadwaiter : public wait::waiter {
 public:
  inline threadwaiter(wait::queue &wq, struct thread *td) : wait::waiter(wq), thd(td) { thd->waiter = this; }

  virtual ~threadwaiter(void) { thd->waiter = nullptr; }

  virtual bool notify(int flags) override {
    // if (flags & NOTIFY_RUDE) printk("rude!\n");
    thd->awaken(flags);
    // I absorb this!
    return true;
  }

  virtual void start(void) override { sched::do_yield(PS_BLOCKED); }

  struct thread *thd = NULL;
};

void wait::waiter::interrupt(void) { wq.interrupt(this); }

void wait::queue::interrupt(wait::waiter *w) {
  scoped_lock l(lock);
  if (w->flags & WAIT_NOINT) return;
}


bool wait::queue::wait(u32 on, ref<wait::waiter> wt) { return do_wait(on, 0, wt); }

void wait::queue::wait_noint(u32 on, ref<waiter> wt) { do_wait(on, WAIT_NOINT, wt); }

bool wait::queue::do_wait(u32 on, int flags, ref<wait::waiter> waiter) {
  if (!waiter) {
    waiter = make_ref<threadwaiter>(*this, curthd);
  }
  lock.lock();

  if (navail > 0) {
    navail--;
    lock.unlock();
    return true;
  }

  assert(navail == 0);

  waiter->flags = flags;

  waiter->next = NULL;
  waiter->prev = NULL;

  curthd->waiter = waiter.get();

  if (!back) {
    assert(!front);
    back = front = waiter;
  } else {
    back->next = waiter;
    waiter->prev = back;
    back = waiter;
  }

  lock.unlock();

  waiter->start();

  // TODO: read from the thread if it was rudely notified or not
  return curthd->wq.rudely_awoken == false;
}

void wait::queue::notify(int flags) {
  scoped_lock lck(lock);
top:
  if (!front) {
    navail++;
  } else {
    auto waiter = front;
    if (front == back) back = nullptr;
    front = waiter->next;
    // *nicely* awaken the thread
    if (!waiter->notify(flags)) {
      goto top;
    }
  }
}

void wait::queue::notify_all(int flags) {
  scoped_lock lck(lock);

  if (!front) {
    navail++;
  } else {
    while (front) {
      auto waiter = front;
      if (front == back) back = nullptr;
      front = waiter->next;
      // *nicely* awaken the thread
      waiter->notify(flags);
    }
  }
}

bool wait::queue::should_notify(u32 val) {
  lock.lock();
  if (front) {
    if (front->waiting_on <= val) {
      lock.unlock();
      return true;
    }
  }
  lock.unlock();
  return false;
}

