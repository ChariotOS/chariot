#include <cpu.h>
#include <wait.h>


wait_entry::wait_entry() {
  wq = NULL;
  thd = curthd;
}

wait_entry::~wait_entry() {
  if (wq != NULL) {
    wq->finish(this);
  }
}




wait_queue::wait_queue(void) { task_list.init(); }

void wait_queue::wake_up_common(unsigned int mode, int nr_exclusive, int wake_flags, void *key) {
  struct wait_entry *curr, *next;
  list_for_each_entry_safe(curr, next, &task_list, item) {
    unsigned flags = curr->flags;
    if (curr->func(curr, mode, wake_flags, key) && (flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive) {
      break;
    }
  }
}


wait_result wait_queue::wait(struct wait_entry *ent, int state) {
  assert(ent->wq == NULL);

  sched::set_state(state);

  unsigned long flags;
  ent->wq = this;
  ent->flags &= ~WQ_FLAG_EXCLUSIVE;
  flags = lock.lock_irqsave();
  task_list.add(&ent->item);
  lock.unlock_irqrestore(flags);

  sched::yield();

  return wait_result(ent->result);
}


wait_result wait_queue::wait(void) {
  struct wait_entry entry;
  return wait(&entry, PS_INTERRUPTIBLE);
}



wait_result wait_queue::wait_exclusive(struct wait_entry *ent, int state) {
  assert(ent->wq == NULL);

  sched::set_state(state);

  unsigned long flags;
  ent->wq = this;
  ent->flags |= WQ_FLAG_EXCLUSIVE;
  flags = lock.lock_irqsave();
  task_list.add_tail(&ent->item);
  lock.unlock_irqrestore(flags);

  sched::yield();

  return wait_result(ent->result);
}


wait_result wait_queue::wait_exclusive(void) {
  struct wait_entry entry;
  return wait_exclusive(&entry, PS_INTERRUPTIBLE);
}



void wait_queue::wait_noint(void) {
  struct wait_entry entry;
  wait(&entry, PS_UNINTERRUPTIBLE);
}


void wait_queue::finish(struct wait_entry *e) {
  assert(e->wq == this);

  unsigned long flags;
  e->thd->state = PS_RUNNING;
  // "carefully" check if the entry's linkage is empty or not.
  // If it is, then we lock the waitqueue and unlink it
  if (!e->item.is_empty_careful()) {
    flags = lock.lock_irqsave();
    e->item.del_init();
    lock.unlock_irqrestore(flags);
  }
  e->wq = NULL;
}


bool autoremove_wake_function(struct wait_entry *entry, unsigned mode, int sync, void *key) {
  bool ret = true;

  sched::unblock(*entry->thd, false);
  entry->result = 0;
  if (entry->thd->wq.rudely_awoken) {
    entry->result |= WAIT_RES_INTR;
  }

  entry->flags |= WQ_FLAG_RUDELY;
  if (ret) entry->item.del_init();
  return ret;
}
