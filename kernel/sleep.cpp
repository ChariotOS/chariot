#include <cpu.h>
#include <errno.h>
#include <sleep.h>
#include <time.h>

void remove_sleep_blocker(struct processor_state &cpu, struct sleep_blocker *blk) {
  if (blk == cpu.sleepers) cpu.sleepers = blk->next;
  if (blk->next != NULL) blk->next->prev = blk->prev;
  if (blk->prev != NULL) blk->prev->next = blk->next;
}

void add_sleep_blocker(struct processor_state &cpu, struct sleep_blocker *blk) {
  /* Insert the sleep node to the start of the cpu.sleepers linked list */
  blk->next = cpu.sleepers;
  blk->prev = NULL;
  if (cpu.sleepers != NULL) {
    cpu.sleepers->prev = blk;
  }
  cpu.sleepers = blk;
}

int do_usleep(uint64_t us) {
  auto &cpu = cpu::current();

  struct sleep_blocker blocker;
  blocker.wakeup_us = time::now_us() + us;

  auto flags = cpu.sleepers_lock.lock_irqsave();
	add_sleep_blocker(cpu, &blocker);
  cpu.sleepers_lock.unlock_irqrestore(flags);

  /* Annoyingly, if we are interrupted, we need to remove the node from the linked list... */
  if (blocker.wq.wait().interrupted()) {
    /* We gotta remove it! Ouch! */
    flags = cpu.sleepers_lock.lock_irqsave();
    remove_sleep_blocker(cpu, &blocker);

    cpu.sleepers_lock.unlock_irqrestore(flags);
    return -EINTR;
  }

  return 0;
}



void check_wakeups_r(void) {
  auto now = time::now_us();
  auto &cpu = cpu::current();

  struct sleep_blocker *blk = cpu.sleepers;
  while (blk != NULL) {
    /* Grab the next now, as this node might be removed */
    auto *next = blk->next;

    /* If a node needs to be woken up, do it :) */
    if (blk->wakeup_us <= now) {
      /* Gotta remove the node from the linked list */
      remove_sleep_blocker(cpu, blk);
      /* Wake them up! */
      blk->wq.wake_up_all();
    }

    /* Continue the loop */
    blk = next;
  }
}


void check_wakeups(void) {
  if (!time::stabilized()) return;
  if (cpu::get() == NULL) return;
  auto &cpu = cpu::current();
  auto flags = cpu.sleepers_lock.lock_irqsave();

  check_wakeups_r();

  cpu.sleepers_lock.unlock_irqrestore(flags);
}
