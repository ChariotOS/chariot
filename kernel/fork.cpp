#include <cpu.h>
#include <sched.h>
#include <syscall.h>


extern "C" void trapret(void);

static void trap_return(void) {

	return;
}


static pid_t do_fork(struct process &p) {
  auto new_mm = p.mm->fork();
  delete new_mm;

  auto np = sched::proc::spawn_process(&p, SPAWN_FORK);

  auto old_td = curthd;
  auto new_td = new thread(np->pid, *np);

  // copy the trapframe
  memcpy(new_td->trap_frame, old_td->trap_frame, arch::trapframe_size());
  new_td->trap_frame[0] = 0;  // return value for child is 0


	// copy floating point if we need to
  if (old_td->fpu.initialized) {
    memcpy(new_td->fpu.state, old_td->fpu.state, 512);
    new_td->fpu.initialized = old_td->fpu.initialized;
  }

	// go to the trap_return function instead of whatever it was gonna do otherwise
	new_td->kern_context->eip = (u64)trap_return;

	new_td->state = PS_RUNNABLE;
	sched::add_task(new_td);

  // return the child pid to the parent
  return np->pid;
}


int sys::fork(void) { return do_fork(*curproc); }
