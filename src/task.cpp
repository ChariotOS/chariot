#include <fs/file.h>
#include <map.h>
#include <printk.h>
#include <task.h>

static mutex_lock proc_table_lock("process_table");
static u64 next_pid = 0;
static map<int, ref<struct task_process>> proc_table;

static mutex_lock task_table_lock("task_table");
static u64 next_tid = 0;
static map<int, ref<struct task>> task_table;

ref<struct task> task::create(void) { return nullptr; }

ref<struct task> task::lookup(int tid) {
  task_table_lock.lock();
  auto t = task_table.get(tid);
  task_table_lock.unlock();
  return t;
}

int task_process::create_task(int (*fn)(void *), void *stack, int flags,
                              void *arg) {
  return 0;
}

ref<struct task_process> task_process::spawn(string path, int uid, int gid,
                                             pid_t parent_pid, int &error,
                                             vec<string> &&args, int spwn_flags, int ring) {
  proc_table_lock.lock();
  pid_t pid = next_pid++;

  KINFO("spawned process '%s' at pid %d user=(%d,%d)\n", path.get(), pid, uid,
        gid);

  assert(!proc_table.contains(pid));

  auto p = make_ref<task_process>();

  p->uid = uid;
  p->gid = gid;
  p->ring = ring;

  auto cmd_path = fs::path(path);

  p->command_path = move(path);
  p->args = args;

  p->parent = proc_table.get(parent_pid);

  p->mm = make_ref<vm::addr_space>();

  if (p->parent) {
    p->cwd = p->parent->cwd;
  }

  proc_table[pid] = p;
  proc_table_lock.unlock();

  return p;
}

ref<struct task_process> task_process::lookup(int pid) {
  proc_table_lock.lock();
  auto t = proc_table.get(pid);
  proc_table_lock.unlock();
  return t;
}
