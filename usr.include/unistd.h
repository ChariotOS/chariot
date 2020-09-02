#ifndef _UNISTD_H
#define _UNISTD_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_ssize_t
#define __NEED_size_t
#define __NEED_off_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_off_t
#define __NEED_pid_t
#define __NEED_intptr_t
#define __NEED_useconds_t
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
int ftruncate(int fildes, off_t length);
int fsync(int fd);

pid_t gettid(void);
pid_t getpid(void);


uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);

int chdir(const char *path);

extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char *const argv[], const char *optstring);

pid_t fork(void);


#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

int access(const char *, int);
int faccessat(int, const char *, int, int);

int chdir(const char *);
int fchdir(int);
char *getcwd(char *, size_t);



int dup(int fildes);
int dup2(int fildes, int fildes2);



int usleep(unsigned long usec);


int unlink(const char *path);



int execl(const char *path, const char *arg0, ... /*, (char *)0 */);
int execle(const char *path, const char *arg0, ... /*,
           (char *)0, char *const envp[]*/);
int execlp(const char *file, const char *arg0, ... /*, (char *)0 */);
int execv(const char *path, char *const argv[]);
// the systemcall
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int execvpe(const char *file, char *const argv[], char *const envp[]);

#ifdef __cplusplus
}
#endif

#endif
