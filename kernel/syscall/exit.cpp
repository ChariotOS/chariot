#include <cpu.h>
#include <process.h>

void sys::exit_task(int code) {
  curtask->state = PS_ZOMBIE;

  // assume this task is the last task
  int last_thread = true;

  for (auto &t : curproc->tasks) {
    if (t == curtask->tid) continue;
    auto tsk = task::lookup(t);

    if (tsk != NULL) {
      if (tsk->state != PS_ZOMBIE) {
        last_thread = false;
        break;
      }
    }
  }

  printk("exit_task %d\n", curtask->tid);

  if (last_thread || curproc->tasks[0] == curtask->tid) {
    // need to signal to the parent process that whole process died.
    printk("last thread\n");
  }

  curtask->exit_code = code;
  curtask->should_die = true;
  sched::exit();
}

void sys::exit_proc(int code) {
  auto proc = cpu::proc();
  auto mytask = cpu::task();

  assert(proc != NULL);
  assert(mytask != NULL);

  printk("[exit] exiting pid %d\n", proc->pid);

  /* force all threads/tasks in this process to exit. They don't need to be
   * reaped.
   */
  for (auto tid : proc->tasks) {
    auto *task = task::lookup(tid);
    if (task != NULL && task != mytask) {
      printk("task [%d] exit\n", task->tid);
      task->exit(code);
    }
  }

  printk("Exit proc. code=%d\n", code);
  sys::exit_task(code);
}

