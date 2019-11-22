#ifndef _UNISTD_H
#define _UNISTD_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_ssize_t
#define __NEED_size_t
#include <bits/alltypes.h>

// read or write from a file descriptor given a buffer of data

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);


#ifdef __cplusplus
}
#endif

#endif
