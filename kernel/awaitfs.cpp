#include <awaitfs.h>
#include <cpu.h>
#include <errno.h>
#include <ck/map.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <wait.h>

using table_key_t = off_t;

void poll_table::wait(wait_queue &wq, short events) {
  if (!locked) {
    wq.lock.lock_irqsave();
    ents.push({&wq, events});
  }
}


struct await_table_entry {
  ck::ref<fs::file> file;
  // what are we waiting on?
  short awaiting;
  short event;

  int index = 0;
  /* The table we pass to the file desc to poll */
  poll_table pt;
};



struct await_table_metadata {
  poll_table_entry *pte;
  await_table_entry *awe;
};



static inline void unlock_awaitfs_ents(ck::vec<await_table_entry> &ents, bool irqs_enabled) {
  for (auto &e : ents) {
    for (auto &p : e.pt.ents) {
      p.wq->lock.unlock_irqrestore(irqs_enabled);
    }
  }
}

int sys::awaitfs(struct await_target *targs, int nfds, int flags, long long timeout_time) {
  if (nfds == 0) return -EINVAL;
  if (!curproc->mm->validate_pointer(targs, sizeof(*targs) * nfds, PROT_READ | PROT_WRITE))
    return -1;

  bool irqs_enabled = arch_irqs_enabled();

  unsigned nqueues = 0;
  ck::vec<await_table_entry> entries;
  /* These are both kept in line with eachother. We just simply need a contiguous array of queues
   * for multi_wait */
  ck::vec<wait_queue *> queues;
  ck::vec<await_table_metadata> metadata;

  /* To avoid reallocation */
  entries.ensure_capacity(nfds);

  // build up the entry list
  for (int i = 0; i < nfds; i++) {
    auto &targ = targs[i];
    auto file = curproc->get_fd(targ.fd);

    if (file) {
      struct await_table_entry entry;
      entry.awaiting = targ.awaiting;
      entry.event = 0;
      entry.index = i;
      entry.file = file;

      int immediate_events = file->ino->poll(*entry.file, entry.awaiting, entry.pt);

      nqueues += entry.pt.ents.size();
      entries.push(entry);


      // /* Optimization. If someone is already ready, return such */
      if (immediate_events & entry.awaiting) {
        targs[i].occurred = immediate_events;
        unlock_awaitfs_ents(entries, irqs_enabled);
        return i;
      }
    }
  }

  /* If there is a timer, add it on the end of the queues, so we need one more! */
  if ((long long)timeout_time > 0) nqueues += 1;

  queues.ensure_capacity(nqueues);
  metadata.ensure_capacity(nqueues);

  for (auto &ent : entries) {
    for (auto &p : ent.pt.ents) {
      queues.push(p.wq);
      metadata.push({
          .pte = &p,
          .awe = &ent,
      });
    }
  }

  // uninitialized sleep waiter
  sleep_waiter sw;
  sw.cpu = NULL;
  int timer_index = -1;
  if ((long long)timeout_time > 0) {
    auto now_ms = time::now_ms();
    long long to_go = timeout_time - now_ms;
    if (to_go <= 0) {
      unlock_awaitfs_ents(entries, irqs_enabled);
      return -ETIMEDOUT;
    }
    sw.start(to_go * 1000);
    timer_index = queues.size();
    queues.push(&sw.wq);
    metadata.push({NULL, NULL}); /* idk */
  }


  int res = multi_wait_prelocked(queues.data(), queues.size(), irqs_enabled, true);

  if (res == -EINTR) return -EINTR;
  /* Check for timeout */
  if (timeout_time && res == timer_index) {
    return -ETIMEDOUT;
  }

  assert(res < queues.size());

  int index = metadata[res].awe->index;

  /* Update the target entry */
  targs[index].occurred = metadata[res].pte->events;
  return index;
}
