#pragma once

#include <dirent.h>
#include <errno.h>
#include <fs.h>
#include <fs/vfs.h>
#include <ck/func.h>
#include <lock.h>
#include <sched.h>
#include <stat.h>
#include <ck/string.h>

#include <fcntl.h>
#include <awaitfs.h>

#define RING_KERNEL 0
#define RING_USER 3


#define SYSSYM(name) sys_##name
#include <net/socket.h>

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




#include <syscall.def.h>
