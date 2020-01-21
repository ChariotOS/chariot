#ifndef _UNISTD_H
#define _UNISTD_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_ssize_t
#define __NEED_size_t
#define __NEED_off_t
#include <bits/alltypes.h>

// read or write from a file descriptor given a buffer of data

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);


int close(int fd);

#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)

off_t lseek(int fd, off_t offset, int whence);


#ifdef __cplusplus
}
#endif

#endif
