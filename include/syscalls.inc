__SYSCALL(0, restart) // TODO

// exit the current task
__SYSCALL(1, exit_task)
__SYSCALL(2, open)
__SYSCALL(3, close)
__SYSCALL(4, lseek)
__SYSCALL(5, read)
__SYSCALL(6, write)
__SYSCALL(7, yield)


// process creation and various related systemcalls
__SYSCALL(8, pctl)
// __SYSCALL(8, spawn)
// __SYSCALL(9, cfgpid)
// __SYSCALL(10, cmdpidve)


__SYSCALL(9, dup)
__SYSCALL(10, dup2)


// __SYSCALL(11, stat)
__SYSCALL(12, fstat)
// __SYSCALL(13, lstat)

// exit the whole process
__SYSCALL(42, exit_proc)
__SYSCALL(60, mmap)
__SYSCALL(61, munmap)

