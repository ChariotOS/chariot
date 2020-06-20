// This file is the source of truth for system calls in Chariot. It
// is "parsed" by tools/gen_syscalls.py and the resulting data is used
// to generate various systemcall tables for the kernel and the userspace.
//
// A triple slash before the syscall allows the definition of an attribute,
// where `num` is the systemcall number (the value of eax when called)

#include <types.h>

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

/// num=0x08
int fork(void);

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

/// num=0x1a
int dup2(int oldfd, int newfd);

/// num=0x1b
int chdir(const char *);

/// num=0x1c
int getcwd(char *dst, int dlen);

/// num=0x1d
int chroot(const char *);

/// num=0x1f
int ioctl(int fd, int cmd, unsigned long value);

/// num=0x20
int yield(void);

/// num=0x21
int getpid(void);

/// num=0x22
int gettid(void);

/// num=0x26
long getuid(void);
/// num=0x27
long geteuid(void);
/// num=0x28
long getgid(void);
/// num=0x29
long getegid(void);

/// num=0x30
void *mmap(void *addr, long length, int prot, int flags, int fd, long offset);

/// num=0x31
int munmap(void *addr, unsigned long length);

/// num=0x32
int mrename(void *addr, char *name);

/// num=0x33
int mgetname(void *addr, char *name, size_t bufsz);

/// num=0x34
int mregions(struct mmap_region *, int nregions);

/// num=0x35
unsigned long mshare(int action, void *arg);

/// num=0x40
int dirent(int fd, struct dirent *, int offset, int count);

/// num=0x50
time_t localtime(struct tm *tloc);
/// num=0x51
size_t gettime_microsecond(void);

/// num=0x52
int usleep(unsigned long usec);

/// num=0x60
int socket(int domain, int type, int protocol);
/// num=0x61
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr*, size_t addrlen);

/// num=0x62
ssize_t recvfrom(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr*, size_t addrlen);

/// num=0x63
int bind(int sockfd, const struct sockaddr*, size_t addrlen);

/// num=0x64
int accept(int sockfd, struct sockaddr *, int addrlen);

/// num=0x65
int connect(int sockfd, const struct sockaddr *addr, int addrlen);


/* signal related system calls own 0x7X */

/// num=0x70
int signal_init(void *sigret);

/// num=0x71
int signal(int sig, void *handler);
/// num=0x72
int sigreturn(void);

/// num=0x73
int sigprocmask(int how, unsigned long set, unsigned long *old_set);

/// num=0x80
int awaitfs(struct await_target *, int nfds, int flags, unsigned long timeout_time);

/// num=0xF0
unsigned long kshell(char *cmd, int argc, char **argv, void *data, size_t len);
