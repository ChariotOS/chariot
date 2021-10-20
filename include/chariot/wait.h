#pragma once

#include <list_head.h>
#include <lock.h>
#include <ck/ptr.h>
#include <types.h>
#include <ck/vec.h>

bool autoremove_wake_function(struct wait_entry *entry, unsigned mode, int sync, void *key);

#define WAIT_RES_INTR 0b00000001
#define WAIT_RES_TIMEOUT 0b00000010

struct wait_result {
 private:
  uint8_t flags;

 public:
  inline wait_result(uint8_t flags) : flags(flags) {}
  inline wait_result(void) : flags(0) {}
  inline bool interrupted(void) { return FIS(flags, WAIT_RES_INTR); }
  inline bool timed_out(void) { return FIS(flags, WAIT_RES_TIMEOUT); }
};

/* these functions return if they were successful in waking or not. */
typedef bool (*wait_entry_func_t)(struct wait_entry *wait, unsigned mode, int flags, void *key);
struct wait_entry {
  unsigned int flags = 0;
#define WQ_FLAG_EXCLUSIVE 0x01
#define WQ_FLAG_RUDELY 0x02
  /* The thread that is waiting on the waitqueue */
  ck::ref<Thread> thd;
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

  int result = 0; /* WQ_RES_* flags */

  wait_entry();
  // this removes the entry from a waitqueue if there is one
  ~wait_entry();

  wait_result start(spinlock *held_lock = nullptr);
};




struct wait_queue {
  spinlock lock;

  struct list_head task_list;

  wait_queue();


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
    bool en = lock.lock_irqsave();
    wake_up_common(mode, nr_exclusive, 0, key);
    lock.unlock_irqrestore(en);
  }


  inline void wake_up(void) { __wake_up(0, 1, NULL); }
  inline void wake_up_all(void) { __wake_up(0, 0, NULL); }


  wait_result wait_exclusive(struct wait_entry *, int state);
  wait_result wait_exclusive();

  wait_result wait(struct wait_entry *, int state);
  wait_result wait();


  void add(struct wait_entry *);
  void remove(struct wait_entry *);

  wait_result wait_timeout(long long us);

  void wait_noint();
};


/* Wait for several wait queues, returning the index of the queue that woke first, or -EINTR */
int multi_wait(wait_queue **queues, size_t nqueues, bool exclusive = false);
int multi_wait_prelocked(wait_queue **queues, size_t nqueues, bool irqs_enabled, bool exclusive = false);




/*
 * The usage of these functions is as follows:
 * static wait_queue wq; // global
 *
 * struct wait_entry ent;
 * for (;;) {
 *   prepare_to_wait(wq, ent, true);
 *   if (cond) break;
 *   sched::yield();
 * }
 * finish_wait();
 */
void prepare_to_wait(struct wait_queue &wq, struct wait_entry &ent, bool interruptible = true);
void prepare_to_wait_exclusive(struct wait_queue &wq, struct wait_entry &ent, bool interruptible = true);
wait_result do_wait(struct wait_entry &ent);
void finish_wait(struct wait_queue &wq, struct wait_entry &ent);
