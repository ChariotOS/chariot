#include <cpu.h>
#include <syscall.h>

void sys::exit_thread(int code) {
  // if we are the main thread, exit the group instead.
  if (curthd->pid == curthd->tid) {
    sys::exit_proc(code);
    return;
  }

  // ...
  // printk("exit_thread(%d)\n", code);

  // TODO: notify the parent

  // defer to later!
  curthd->should_die = 1;
  curproc->waiters.post();

  sched::exit();
}

void sys::exit_proc(int code) {
  // ...
  // printk("exit_proc(%d)\n", code);

  {
    scoped_lock l(curproc->datalock);
    for (auto tid : curproc->threads) {
      auto t = thread::lookup(tid);
      if (t && t != curthd) {
        t->should_die = 1;
				sched::unblock(*t, true);
      }
    }

    /* all the threads should be rudely awoken now... */
    for (auto tid : curproc->threads) {
      auto t = thread::lookup(tid);
      if (t && t != curthd) {
        // take the run lock for now... This ensures that the thread has completed.
        t->locks.run.lock();
        assert(t->state != PS_ZOMBIE);
        t->locks.run.unlock();
      }
    }
  }


  curproc->exit_code = code;
  curproc->exited = true;
  curproc->parent->waiters.post();

  sched::proc::send_signal(curproc->parent->pid, SIGCHLD);

  sched::exit();
}

