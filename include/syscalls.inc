__SYSCALL(0, restart) // TODO
__SYSCALL(1, exit)
__SYSCALL(2, open)
__SYSCALL(3, close)
__SYSCALL(4, lseek)
__SYSCALL(5, read)
__SYSCALL(6, write)
__SYSCALL(7, yield)


// process creationg related systemcalls
__SYSCALL(8, spawn)
__SYSCALL(9, impersonate)
__SYSCALL(10, cmdpidve)

