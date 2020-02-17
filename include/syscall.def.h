// This file is the source of truth for system calls in Chariot. It
// is "parsed" by tools/gen_syscalls.py and the resulting data is used
// to generate various systemcall tables for the kernel and the userspace.
//
// A triple slash before the syscall allows the definition of an attribute,
// where `num` is the systemcall number (the value of eax when called)

/// num=0x00
void restart(void);

/// num=0x01
void exit_thread(int code);

/// num=0x02
void exit_proc(int code);

/// num=0x03
int spawn(void);

/// num=0x04
int despawn(int);

/// num=0x05
int startpidve(int pid, const char *path, const char *argv[], const char *envp[]);

/// num=0x06
int pctl(int pid, int cmd, unsigned long arg);

/// num=0x07
long waitpid(int pid, int *stat, int options);

/// num=0x10
int open(const char *path, int flags, int mode = 0);

/// num=0x11
int close(int fd);

/// num=0x12
long lseek(int fd, long offset, int whence);

/// num=0x13
long read(int fd, void *, long);

/// num=0x14
long write(int fd, void *, long);

/// num=0x16
int stat(const char *pathname, struct stat *statbuf);

/// num=0x17
int fstat(int fd, struct stat *statbuf);

/// num=0x18
int lstat(const char *pathname, struct stat *statbuf);

/// num=0x19
int dup(int fd);

/// num=0x20
int dup2(int oldfd, int newfd);


/// num=0x20
int yield(void);

/// num=0x21
int getpid(void);

/// num=0x22
int gettid(void);


/// num=0x30
void *mmap(void *addr, long length, int prot, int flags, int fd, long offset);

/// num=0x31
int munmap(void *addr, unsigned long length);

/// num=0x32
int mrename(void *addr, char *name);


/// num=0x40
int dirent(int fd, struct dirent *, int offset, int count);
