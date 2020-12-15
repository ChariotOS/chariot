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




void wait_queue::wake_up_common(unsigned int mode, int nr_exclusive, int wake_flags, void *key) {
  struct wait_entry *curr, *next;
  list_for_each_entry_safe(curr, next, &task_list, item) {
    unsigned flags = curr->flags;
    if (curr->func(curr, mode, wake_flags, key) && (flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive) {
      break;
    }
  }
}

void wait_queue::wait(struct wait_entry *ent, int state) {
	assert(ent->wq == NULL);

  unsigned long flags;
	ent->wq = this;
  ent->flags &= ~WQ_FLAG_EXCLUSIVE;
  flags = lock.lock_irqsave();
  if (task_list.is_empty()) {
    task_list.add(&ent->item);
  }
  lock.unlock_irqrestore(flags);

  sched::do_yield(state);
}


bool wait_queue::wait(void) {
  struct wait_entry entry;
  wait(&entry, PS_INTERRUPTIBLE);

  bool rude = (entry.flags & WQ_FLAG_RUDELY) != 0;
  return rude;
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

  if (entry->thd->awaken()) {
    entry->flags |= WQ_FLAG_RUDELY;
  }
  if (ret) entry->item.del_init();
  return ret;
}
