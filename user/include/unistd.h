#ifndef _UNISTD_H
#define _UNISTD_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_ssize_t
#define __NEED_size_t
#define __NEED_off_t
#define __NEED_pid_t
#include <bits/alltypes.h>

// read or write from a file descriptor given a buffer of data

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);


int close(int fd);

/**
 * exit()
 *
 * Exit the process group
 */
void exit(int status);

#undef SEEK_SET
#undef SEEK_CUR
#undef SEEK_END
#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)

off_t lseek(int fd, off_t offset, int whence);


pid_t gettid(void);
pid_t getpid(void);


int chdir(const char *path);


extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char * const argv[], const char *optstring);


#ifdef __cplusplus
}
#endif

#endif
