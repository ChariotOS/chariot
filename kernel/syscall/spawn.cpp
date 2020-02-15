#include <cpu.h>
#include <process.h>
#include <sched.h>

int sys::spawn(void) {
  auto proc = curproc;

  struct sched::proc::spawn_options opts;
  opts.ring = RING_USER;

  auto np = sched::proc::spawn_process(proc, opts);

  if (np) {
    proc->datalock.lock();
    // store it in the embryos
    proc->embryos.push(np);
    proc->datalock.unlock();
    return np->pid;
  }
  return -1;
}
