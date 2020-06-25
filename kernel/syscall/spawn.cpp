#include <cpu.h>
#include <syscall.h>
#include <sched.h>

int sys::spawn(void) {
  auto proc = curproc;

  struct sched::proc::spawn_options opts;
  opts.ring = RING_USER;

  auto np = sched::proc::spawn_process(proc, 0);

  if (np) {
    proc->datalock.lock();
    // auto leak = proc->mm->fork();
    // delete leak;

    // store it in the embryos
    proc->embryos.push(np);
    proc->datalock.unlock();
    return np->pid;
  }
  return -1;
}

int sys::despawn(int pid) {
  auto proc = curproc;
  scoped_lock lck(proc->datalock);

  int emb;
  process::ptr np;
  for (emb = 0; emb < proc->embryos.size(); emb++) {
    auto p = proc->embryos[emb];
    assert(p);

    if (p->pid == pid) {
      np = p;
      break;
    }
  }

  if (!np) return -ENOENT;

  proc->embryos.remove(emb);
  sched::proc::ptable_remove(pid);
  return 0;
}


