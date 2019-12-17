#pragma once

#include <fs.h>
#include <func.h>
#include <lock.h>
#include <sched.h>
#include <string.h>
#include <syscalls.h>
#include <vm.h>
#include <stat.h>

#define RING_KERNEL 0
#define RING_USER 3

void syscall_init(void);
long ksyscall(long n, ...);

#define SYSSYM(name) sys_##name

/**
 * the declaration of every syscall function. The kernel should go though this
 * interface to use them when running kernel processes/threads
 *
 * These functions are implemented in process.cpp
 */
namespace sys {

void restart(void);
void exit(void);

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 00200000
#define O_NOFOLLOW 00400000
#define O_CLOEXEC 02000000
#define O_NOFOLLOW_NOERROR 0x4000000
int open(const char *path, int flags, int mode = 0);

int close(int fd);

off_t lseek(int fd, off_t offset, int whence);

ssize_t read(int fd, void *, size_t);
ssize_t write(int fd, void *, size_t);

int yield(void);

pid_t getpid(void);

pid_t gettid(void);

int pctl(int pid, int request, u64 arg);



int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);


int dup(int fd);
int dup2(int oldfd, int newfd);

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset);
int munmap(void *addr, size_t length);

}  // namespace sys
