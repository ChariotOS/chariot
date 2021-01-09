#include <cpu.h>
#include <errno.h>
#include <sleep.h>
#include <time.h>


sleep_waiter::~sleep_waiter() { this->remove(); }

void sleep_waiter::start(uint64_t us) {
  wakeup_us = time::now_us() + us;
  cpu = cpu::get();

  auto flags = cpu->sleepers_lock.lock_irqsave();
  /* Insert the sleep node to the start of the cpu.sleepers linked list */
  next = cpu->sleepers;
  prev = NULL;
  if (cpu->sleepers != NULL) {
    cpu->sleepers->prev = this;
  }
  cpu->sleepers = this;
  cpu->sleepers_lock.unlock_irqrestore(flags);
}

wait_result sleep_waiter::wait(void) {
  if (cpu == NULL) {
    panic("sleep_waiter waited on without being bound\n");
  }
  return wq.wait();
}

void sleep_waiter::remove(void) {
  /* If there is no CPU, we aren't added */
  if (cpu == NULL) {
    return;
  }


  auto flags = cpu->sleepers_lock.lock_irqsave();
  if (this == cpu->sleepers) cpu->sleepers = this->next;
  if (this->next != NULL) this->next->prev = this->prev;
  if (this->prev != NULL) this->prev->next = this->next;

  cpu->sleepers_lock.unlock_irqrestore(flags);
}

void remove_sleep_waiter(struct processor_state &cpu, struct sleep_waiter *blk) {}

void add_sleep_waiter(struct processor_state &cpu, struct sleep_waiter *blk) {}

int do_usleep(uint64_t us) {
  struct sleep_waiter blocker;
  blocker.start(us);

  if (blocker.wait().interrupted()) {
    return -EINTR;
  }

  return 0;
}



bool check_wakeups_r(void) {
  auto now = time::now_us();
  auto &cpu = cpu::current();


  bool found = false;
  struct sleep_waiter *blk = cpu.sleepers;
  while (blk != NULL) {
    /* Grab the next now, as this node might be removed */
    auto *next = blk->next;

    /* If a node needs to be woken up, do it :) */
    if (blk->wakeup_us <= now) {
      /* Wake them up! */
      blk->wq.wake_up_all();
      found = true;
    }

    /* Continue the loop */
    blk = next;
  }

  return found;
}


bool check_wakeups(void) {
  if (!time::stabilized()) return false;
  if (cpu::get() == NULL) return false; /* if we get an interrupt before initializing the cpu? */
  auto &cpu = cpu::current();

  if (cpu.sleepers_lock.is_locked()) return false;

  auto flags = cpu.sleepers_lock.lock_irqsave();

  auto b = check_wakeups_r();

  cpu.sleepers_lock.unlock_irqrestore(flags);

  return b;
}
