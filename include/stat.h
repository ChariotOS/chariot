#pragma once

#ifndef __CHARIOT_STAT_H
#define __CHARIOT_STAT_H

// TODO: move to own spot
typedef long time_t;
struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

struct stat {
  unsigned long st_dev;
  unsigned long st_ino;
  unsigned long st_nlink;

  unsigned long st_mode;
  long st_uid;
  long st_gid;

  unsigned int st_rdev;
  unsigned int st_size;
  unsigned int st_blksize;
  unsigned int st_blocks;

  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
};

#endif
