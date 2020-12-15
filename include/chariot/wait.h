#pragma once

#include <list_head.h>
#include <lock.h>
#include <ptr.h>
#include <types.h>


bool autoremove_wake_function(struct wait_entry *entry, unsigned mode, int sync, void *key);

/* these functions return if they were successful in waking or not. */
typedef bool (*wait_entry_func_t)(struct wait_entry *wait, unsigned mode, int flags, void *key);
struct wait_entry {
  unsigned int flags;
#define WQ_FLAG_EXCLUSIVE 0x01
#define WQ_FLAG_RUDELY 0x02
  /* what thread does this wait entry control */
  struct thread *thd;
  /* private data */
  void *_priv = NULL;
  template <typename T>
  T *&priv(void) {
    return (T *&)_priv;
  }

  wait_entry_func_t func = autoremove_wake_function;
  /* what wait queue is this attached to? */
  struct wait_queue *wq;
  /* The inline list_head that is linked into the wait queue */
  struct list_head item;

  wait_entry();
  // this removes the entry from a waitqueue if there is one
  ~wait_entry();
};



struct wait_queue {
  spinlock lock;

  struct list_head task_list;


  void wait(struct wait_entry *, int state);

  void wake_up_common(unsigned int mode, int nr_exclusive, int wake_flags, void *key);

	void finish(struct wait_entry *);


  /**
   * __wake_up - wake up threads blocked on a waitqueue.
   * @mode: which threads
   * @nr_exclusive: how many wake-one or wake-many threads to wake up
   * @key: is directly passed to the wakeup function
   *
   * It may be assumed that this function implies a write memory barrier before
   * changing the task state if and only if any tasks are woken up.
   */
  inline void __wake_up(unsigned int mode, int nr_exclusive, void *key) {
    unsigned long flags = lock.lock_irqsave();
    wake_up_common(mode, nr_exclusive, 0, key);
    lock.unlock_irqrestore(flags);
  }


  inline void wake_up(void) { __wake_up(0, 1, NULL); }
  inline void wake_up_all(void) { __wake_up(0, 0, NULL); }

  bool wait();
  void wait_noint();
};
