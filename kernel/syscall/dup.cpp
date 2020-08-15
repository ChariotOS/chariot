#include <syscall.h>
#include <cpu.h>

int sys::dup(int fd) {
	auto f = curproc->get_fd(fd);
	return curproc->add_fd(f);
}

// copy have newfd refer to oldfd (copy i guess)
int sys::dup2(int oldfd, int newfd) {
	auto f = curproc->get_fd(oldfd);
	if (!f) return -EBADFD;


	sys::close(newfd);

	scoped_lock l(curproc->file_lock);
	curproc->open_files.set(newfd, f);
  return newfd;
}
