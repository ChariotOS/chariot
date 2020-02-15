#include <process.h>
#include <wait_flags.h>
#include <cpu.h>


long sys::waitpid(long pid, int *status_ptr, int flags) {

  if (status_ptr != NULL && !curproc->addr_space->validate_pointer(status_ptr, sizeof(*status_ptr), VPROT_WRITE)) {
    return -1;
  }

  // local status which will be built up as we figure out what to wait on.
  int status = 0;

  int res = sched::proc::do_waitpid(pid, status, flags);

  if (status_ptr != NULL) *status_ptr = status;
  return res;
}






/*
int sys_waitpid(pid_t pid,int * stat_addr, int options)
{
	int flag=0;
	struct task_struct ** p;

	verify_area(stat_addr,4);
repeat:
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p && *p != current &&
		   (pid==-1 || (*p)->pid==pid ||
		   (pid==0 && (*p)->pgrp==current->pgrp) ||
		   (pid<0 && (*p)->pgrp==-pid)))
			if ((*p)->father == current->pid) {
				flag=1;
				if ((*p)->state==TASK_ZOMBIE) {
					put_fs_long((*p)->exit_code,
						(unsigned long *) stat_addr);
					current->cutime += (*p)->utime;
					current->cstime += (*p)->stime;
					flag = (*p)->pid;
					release(*p);
					return flag;
				}
			}
	if (flag) {
		if (options & WNOHANG)
			return 0;
		sys_pause();
		if (!(current->signal &= ~(1<<(SIGCHLD-1))))
			goto repeat;
		else
			return -EINTR;
	}
	return -ECHILD;
}
*/
