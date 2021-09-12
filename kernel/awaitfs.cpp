#include <awaitfs.h>
#include <cpu.h>
#include <errno.h>
#include <ck/map.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <wait.h>
#include "ck/ptr.h"

using table_key_t = off_t;




#define ATOMIC_SET(thing) __atomic_test_and_set((thing), __ATOMIC_ACQUIRE)
#define ATOMIC_CLEAR(thing) __atomic_clear((thing), __ATOMIC_RELEASE)
#define ATOMIC_LOAD(thing) __atomic_load_n((thing), __ATOMIC_RELAXED)



bool poll_table_wake(struct wait_entry *entry, unsigned mode, int sync, void *key) {
  struct poll_table_wait_entry *e = container_of(entry, struct poll_table_wait_entry, entry);

  ATOMIC_SET(&e->awoken);

  return autoremove_wake_function(entry, mode, sync, key);
}


// when a
void poll_table::wait(wait_queue &wq, short events) {
  auto ent = new poll_table_wait_entry();

  ent->wq = &wq;
  ent->events = events;
  ent->table = this;
  ent->entry.func = poll_table_wake;
  // add the entry to this waitqueue
  ent->wq->add(&ent->entry);

  ents.push(move(ent));
}


struct await_table_entry {
  ck::ref<fs::file> file;
  // what are we waiting on?
  short awaiting;

  int index = 0;
  /* The table we pass to the file desc to poll */
  poll_table pt;
};

void free_awaitfs_entries(ck::vec<await_table_entry> &ents) {
  for (auto &e : ents) {
    // remove each entry from their waitqueue if they are in one.
    for (auto &wait_ent : e.pt.ents) {
      if (wait_ent->wq) {
        wait_ent->wq->remove(&wait_ent->entry);
      }
      delete wait_ent;
    }
  }
}



bool check_awaitfs_entries(ck::vec<await_table_entry> &ents, int &index, short &events) {
  index = -1;
  events = 0;
  for (auto &e : ents) {
    for (auto &wait_ent : e.pt.ents) {
      if (ATOMIC_LOAD(&wait_ent->awoken)) {
        wait_result res = wait_ent->entry.result;

        index = e.index;
        events = wait_ent->events;
        if (res.interrupted()) return true;
        return false;
      }
    }
  }

  return false;
}


struct booltoggle {
  bool &b;

  booltoggle(bool &b) : b(b) { b = true; }

  ~booltoggle() { b = false; }
};



static int do_awaitfs(struct await_target *targs, int nfds, int flags, long long timeout_time) {
  unsigned nqueues = 0;

  booltoggle b(curthd->in_awaitfs);

  ck::vec<await_table_entry> entries;

  /* To avoid reallocation */
  entries.ensure_capacity(nfds);



  // build up the entry list
  for (int i = 0; i < nfds; i++) {
    auto &targ = targs[i];
    // get the file referenced by the file descriptor
    auto file = curproc->get_fd(targ.fd);
    // if that file is not
    if (!file) continue;

    struct await_table_entry entry;
    entry.awaiting = targ.awaiting;
    entry.index = i;
    entry.file = file;
    entry.pt.index = i;

    int immediate_events = file->ino->poll(*entry.file, entry.awaiting, entry.pt);

    nqueues += entry.pt.ents.size();
    entries.push(entry);


    // /* Optimization. If someone is already ready, return such */
    if (immediate_events & entry.awaiting) {
      targs[i].occurred = immediate_events;
      free_awaitfs_entries(entries);
      return i;
    }
  }

  /* If there is a timeout specified, we need to add that to the entries list. */

  // uninitialized sleep waiter
  sleep_waiter sw;
  sw.cpu = NULL;
  int timer_index = -1;
  if ((long long)timeout_time > 0) {
    auto now_ms = time::now_ms();
    long long to_go = timeout_time - now_ms;
    if (to_go <= 0) {
      free_awaitfs_entries(entries);
      return -ETIMEDOUT;
    }
    timer_index = entries.size();

    struct await_table_entry entry;
    entry.awaiting = 0;
    entry.index = timer_index;
    entry.file = nullptr;
    entry.pt.index = timer_index;
    // wait on the sleep_waiter's internal wait queue
    entry.pt.wait(sw.wq, 0);
    sw.start(to_go * 1000);
    entries.push(move(entry));
  }


  // now go through each of the waitqueues we got and grab their lock so we can do some checks.
  bool irqs_enabled = false;
  bool locked_one = false;
  arch_disable_ints();

  int awoken_index = -1;
  short awoken_events = 0;



  for (auto &e : entries) {
    // remove each entry from their waitqueue if they are in one.
    for (auto &wait_ent : e.pt.ents) {
      if (wait_ent->wq) {
        wait_ent->wq->lock.lock();
        // if (!locked_one) irqs_enabled = en;
        // locked_one = true;
      }
    }
  }

  __asm__ __volatile__("" : : : "memory");


  /* Set the process state. This means we won't be interrupted */
  sched::set_state(PS_INTERRUPTIBLE);

  check_awaitfs_entries(entries, awoken_index, awoken_events);
  __asm__ __volatile__("" : : : "memory");

  for (auto &e : entries) {
    for (auto &wait_ent : e.pt.ents) {
      if (wait_ent->wq) {
        wait_ent->wq->lock.unlock();
      }
    }
  }


  // reenable interrupts if we disabled them
  arch_enable_ints();

  __asm__ __volatile__("" : : : "memory");

  // if nobody has been awoken yet, we need to sleep and wait for that to happen
  if (awoken_index == -1) sched::yield();

  __asm__ __volatile__("" : : : "memory");

  sched::set_state(PS_RUNNING);
  // now that we have been awoken, check who did it!
  bool rudely = check_awaitfs_entries(entries, awoken_index, awoken_events);

  free_awaitfs_entries(entries);


  if (timeout_time && awoken_index == timer_index) return -ETIMEDOUT;
  if (rudely) return -EINTR;

  /* Update the target entry */
  targs[awoken_index].occurred = awoken_events;
  return awoken_index;
}

int sys::awaitfs(struct await_target *targs, int nfds, int flags, long long timeout_time) {
  if (nfds == 0) return -EINVAL;
  if (!curproc->mm->validate_pointer(targs, sizeof(*targs) * nfds, PROT_READ | PROT_WRITE))
    return -1;

  return do_awaitfs(targs, nfds, flags, timeout_time);
}
