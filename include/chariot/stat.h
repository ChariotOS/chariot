#pragma once

#ifndef __CHARIOT_STAT_H
#define __CHARIOT_STAT_H

#define S_IFMT 0170000

#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000

#define S_TYPEISMQ(buf) 0
#define S_TYPEISSEM(buf) 0
#define S_TYPEISSHM(buf) 0
#define S_TYPEISTMO(buf) 0

#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#define S_ISCHR(mode) (((mode)&S_IFMT) == S_IFCHR)
#define S_ISBLK(mode) (((mode)&S_IFMT) == S_IFBLK)
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#define S_ISFIFO(mode) (((mode)&S_IFMT) == S_IFIFO)
#define S_ISLNK(mode) (((mode)&S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode)&S_IFMT) == S_IFSOCK)

#ifndef S_IRUSR
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG 0070
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO 0007
#endif

#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

// TODO: move to own spot
#ifndef DEFINED_MY_OWN_TIMESPEC
typedef long time_t;
/*
struct timespec {
  time_t tv_sec;
  long tv_nsec;
};
*/
#endif

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

  time_t st_atim;
  time_t st_mtim;
  time_t st_ctim;
};


int mkdir(const char *path, int mode);

#endif
