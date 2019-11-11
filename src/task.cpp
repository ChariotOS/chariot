#include <map.h>
#include <task.h>

static mutex_lock proc_table_lock("process_table");
static map<int, ref<struct task_process>> proc_table;

static mutex_lock task_table_lock("task_table");
static map<int, ref<struct task>> task_table;

ref<struct task> task::create(void) { return nullptr; }

ref<struct task> task::lookup(int tid) {
  task_table_lock.lock();
  auto t = task_table.get(tid);
  task_table_lock.unlock();
  return t;
}

ref<struct task_process> task_process::create(void) { return nullptr; }

ref<struct task_process> task_process::lookup(int pid) {
  proc_table_lock.lock();
  auto t = proc_table.get(pid);
  proc_table_lock.unlock();
  return t;
}
