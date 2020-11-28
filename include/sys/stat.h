#pragma once

#ifndef _LIBC_STAT_H
#define _LIBC_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>



#define DEFINED_MY_OWN_TIMESPEC
// just refer to the chariot provided stat
#include <chariot/stat.h>


// TODO
// int stat(const char *pathname, struct stat *statbuf);
// int lstat(const char *pathname, struct stat *statbuf);

int fstat(int fd, struct stat *statbuf);
int lstat(const char *path, struct stat *statbuf);
int stat(const char *path, struct stat *statbuf);

#endif


#ifdef __cplusplus
}
#endif
