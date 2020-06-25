#include <syscall.h>
#include <sched.h>
#include <cpu.h>



static pid_t do_fork(struct process &p) {
	auto new_mm = p.mm->fork();
	delete new_mm;
	return -1;
}


int sys::fork(void) {
	return do_fork(*curproc);
}
