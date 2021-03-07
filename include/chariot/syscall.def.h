#include <types.h>
#include <mountopts.h>
namespace sys {
  void restart();
  void exit_thread(int code);
  void exit_proc(int code);
  int execve(const char* path, const char** argv, const char** envp);
  long waitpid(int pid, int* stat, int options);
  int fork();
  int spawnthread(void* stack, void* func, void* arg, int flags);
  int jointhread(int tid);
  int sigwait();
  int prctl(int option, unsigned long arg1, unsigned long arg2, unsigned long arg3,
            unsigned long arg4, unsigned long arg5);
  int open(const char* path, int flags, int mode);
  int close(int fd);
  long lseek(int fd, long offset, int whence);
  long read(int fd, void* buf, size_t len);
  long write(int fd, void* buf, size_t len);
  int stat(const char* pathname, struct stat* statbuf);
  int fstat(int fd, struct stat* statbuf);
  int lstat(const char* pathname, struct stat* statbuf);
  int dup(int fd);
  int dup2(int old, int newfd);
  int mkdir(const char* path, int mode);
  int chdir(const char* path);
  int getcwd(char* dst, int len);
  int chroot(const char* path);
  int unlink(const char* path);
  int ioctl(int fd, int cmd, unsigned long value);
  int yield();
  int getpid();
  int gettid();
  int getuid();
  int geteuid();
  int getgid();
  int getegid();
  int setpgid(int pid, int pgid);
  int getpgid(int pid);
  void* mmap(void* addr, long length, int prot, int flags, int fd, long offset);
  int munmap(void* addr, size_t length);
  int mrename(void* addr, char* name);
  int mgetname(void* addr, char* name, size_t sz);
  int mregions(struct mmap_region* regions, int nregions);
  unsigned long mshare(int action, void* arg);
  int dirent(int fd, struct dirent* ent, int offset, int count);
  time_t localtime(struct tm* tloc);
  size_t gettime_microsecond();
  int usleep(unsigned long usec);
  int socket(int domain, int type, int proto);
  ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* addr,
                 size_t addrlen);
  ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, const struct sockaddr* addr,
                   size_t addrlen);
  int bind(int sockfd, const struct sockaddr* addr, size_t addrlen);
  int accept(int sockfd, struct sockaddr* addr, size_t addrlen);
  int connect(int sockfd, const struct sockaddr* addr, size_t addrlen);
  int signal_init(void* sigret);
  int sigaction(int sig, struct sigaction* new_action, struct sigaction* old);
  int sigreturn(void* ucontext);
  int sigprocmask(int how, unsigned long set, unsigned long* old_set);
  int kill(int pid, int sig);
  int awaitfs(struct await_target* fds, int nfds, int flags, long long timeout_time);
  unsigned long kshell(char* cmd, int argc, char** argv, void* data, size_t len);
  int futex(int* uaddr, int op, int val, int val2, int* uaddr2, int val3);
  int sysinfo(struct sysinfo* info);
  int dnslookup(const char* name, unsigned int* ip4);
  int shutdown();
  int mount(struct mountopts* opts);
}  // namespace sys
