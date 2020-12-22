#include <cpu.h>
#include <sched.h>
#include <time.h>

int sched::round_robin::add_task(struct thread *tsk) {
  if (cpu::in_thread()) arch_disable_ints();
  big_lock.lock();

  int ret = add_task_impl(tsk);

  big_lock.unlock();
  if (cpu::in_thread()) arch_enable_ints();
  return ret;
}


int sched::round_robin::remove_task(struct thread *tsk) {
  if (cpu::in_thread()) arch_disable_ints();
  big_lock.lock();

  int ret = remove_task_impl(tsk);

  big_lock.unlock();
  if (cpu::in_thread()) arch_enable_ints();
  return ret;
}

int sched::round_robin::add_task_impl(struct thread *tsk) {
  if (front == nullptr) {
    // this is the only thing in the queue
    front = tsk;
    back = tsk;
    tsk->sched.next = NULL;
    tsk->sched.prev = NULL;
  } else {
    // insert at the end of the list
    back->sched_state.next = tsk;
    tsk->sched_state.next = NULL;
    tsk->sched_state.prev = back;
    // the new task is the end
    back = tsk;
  }

  return 0;
}



int sched::round_robin::remove_task_impl(struct thread *tsk) {
  if (tsk->sched_state.next) {
    tsk->sched_state.next->sched_state.prev = tsk->sched_state.prev;
  }
  if (tsk->sched_state.prev) {
    tsk->sched_state.prev->sched_state.next = tsk->sched_state.next;
  }
  if (back == tsk) back = tsk->sched_state.prev;
  if (front == tsk) front = tsk->sched_state.next;
  return 0;
}


struct thread *sched::round_robin::pick_next(void) {
  struct thread *td = NULL;


  if (cpu::in_thread()) arch_disable_ints();
  big_lock.lock();


  for (struct thread *thd = front; thd != NULL; thd = thd->sched_state.next) {
    /*
if (thd->state == PS_BLOCKED) {
if (thd->blocker != NULL) {
if (thd->blocker->should_unblock(*thd, time::now_us())) {
thd->state = PS_RUNNABLE;
}
}
}
    */

    if (thd->state == PS_RUNNING) {
      td = thd;
      remove_task_impl(td);
      break;
    }
  }


  big_lock.unlock();
  if (cpu::in_thread()) arch_enable_ints();

  return td;
}

void sched::round_robin::dump(void) {
  printk("rr queue: [");
  for (struct thread *thd = front; thd != NULL; thd = thd->sched_state.next) {
    printk("%3d", thd->tid);
    if (thd->sched_state.next != NULL) {
      printk(", ");
    }
  }
  printk("\n");
}

