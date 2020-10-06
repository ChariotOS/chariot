#pragma once

#include <dirent.h>
#include <errno.h>
#include <fs.h>
#include <fs/vfs.h>
#include <func.h>
#include <lock.h>
#include <sched.h>
#include <stat.h>
#include <string.h>

#include <awaitfs.h>

#define RING_KERNEL 0
#define RING_USER 3

void syscall_init(void);
long ksyscall(long n, ...);

#define SYSSYM(name) sys_##name


struct sysinfo {
  long uptime;             /* Seconds since boot */
  unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
  unsigned long totalram;  /* Total usable main memory size */
  unsigned long freeram;   /* Available memory size */
  unsigned long sharedram; /* Amount of shared memory */
  unsigned long bufferram; /* Memory used by buffers */
  unsigned long totalswap; /* Total swap space size */
  unsigned long freeswap;  /* Swap space still available */
  unsigned short procs;    /* Number of current processes */
  char _f[22];             /* Pads structure to 64 bytes */
};


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

#include <syscall.def.h>
