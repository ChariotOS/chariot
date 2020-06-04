#pragma once

#ifndef _LIBC_STAT_H
#define _LIBC_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

// just refer to the chariot provided stat
#include <chariot/stat.h>


// TODO
// int stat(const char *pathname, struct stat *statbuf);
// int lstat(const char *pathname, struct stat *statbuf);

int fstat(int fd, struct stat *statbuf);
int lstat(const char *path, struct stat *statbuf);

#endif


#ifdef __cplusplus
}
#endif
