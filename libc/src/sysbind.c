#include <sys/sysbind.h>
#include <sys/syscall.h>

extern long __syscall_eintr(int num, unsigned long long a, unsigned long long b,
                      unsigned long long c, unsigned long long d,
                      unsigned long long e, unsigned long long f);void sysbind_restart() {
    return (void)__syscall_eintr(0, 0, 0, 0, 0, 0, 0);
}

void sysbind_exit_thread(int code) {
    return (void)__syscall_eintr(1, (unsigned long long)code, 0, 0, 0, 0, 0);
}

void sysbind_exit_proc(int code) {
    return (void)__syscall_eintr(2, (unsigned long long)code, 0, 0, 0, 0, 0);
}

int sysbind_execve(const char* path, const char ** argv, const char ** envp) {
    return (int)__syscall_eintr(3, (unsigned long long)path, (unsigned long long)argv, (unsigned long long)envp, 0, 0, 0);
}

long sysbind_waitpid(int pid, int* stat, int options) {
    return (long)__syscall_eintr(4, (unsigned long long)pid, (unsigned long long)stat, (unsigned long long)options, 0, 0, 0);
}

int sysbind_fork() {
    return (int)__syscall_eintr(5, 0, 0, 0, 0, 0, 0);
}

int sysbind_spawnthread(void * stack, void* func, void* arg, int flags) {
    return (int)__syscall_eintr(6, (unsigned long long)stack, (unsigned long long)func, (unsigned long long)arg, (unsigned long long)flags, 0, 0);
}

int sysbind_open(const char * path, int flags, int mode) {
    return (int)__syscall_eintr(7, (unsigned long long)path, (unsigned long long)flags, (unsigned long long)mode, 0, 0, 0);
}

int sysbind_close(int fd) {
    return (int)__syscall_eintr(8, (unsigned long long)fd, 0, 0, 0, 0, 0);
}

long sysbind_lseek(int fd, long offset, int whence) {
    return (long)__syscall_eintr(9, (unsigned long long)fd, (unsigned long long)offset, (unsigned long long)whence, 0, 0, 0);
}

long sysbind_read(int fd, void* buf, long len) {
    return (long)__syscall_eintr(10, (unsigned long long)fd, (unsigned long long)buf, (unsigned long long)len, 0, 0, 0);
}

long sysbind_write(int fd, void* buf, long len) {
    return (long)__syscall_eintr(11, (unsigned long long)fd, (unsigned long long)buf, (unsigned long long)len, 0, 0, 0);
}

int sysbind_stat(const char* pathname, struct stat* statbuf) {
    return (int)__syscall_eintr(12, (unsigned long long)pathname, (unsigned long long)statbuf, 0, 0, 0, 0);
}

int sysbind_fstat(int fd, struct stat* statbuf) {
    return (int)__syscall_eintr(13, (unsigned long long)fd, (unsigned long long)statbuf, 0, 0, 0, 0);
}

int sysbind_lstat(const char* pathname, struct stat* statbuf) {
    return (int)__syscall_eintr(14, (unsigned long long)pathname, (unsigned long long)statbuf, 0, 0, 0, 0);
}

int sysbind_dup(int fd) {
    return (int)__syscall_eintr(15, (unsigned long long)fd, 0, 0, 0, 0, 0);
}

int sysbind_dup2(int old, int newfd) {
    return (int)__syscall_eintr(16, (unsigned long long)old, (unsigned long long)newfd, 0, 0, 0, 0);
}

int sysbind_chdir(const char* path) {
    return (int)__syscall_eintr(17, (unsigned long long)path, 0, 0, 0, 0, 0);
}

int sysbind_getcwd(char * dst, int len) {
    return (int)__syscall_eintr(18, (unsigned long long)dst, (unsigned long long)len, 0, 0, 0, 0);
}

int sysbind_chroot(const char * path) {
    return (int)__syscall_eintr(19, (unsigned long long)path, 0, 0, 0, 0, 0);
}

int sysbind_unlink(const char * path) {
    return (int)__syscall_eintr(20, (unsigned long long)path, 0, 0, 0, 0, 0);
}

int sysbind_ioctl(int fd, int cmd, unsigned long value) {
    return (int)__syscall_eintr(21, (unsigned long long)fd, (unsigned long long)cmd, (unsigned long long)value, 0, 0, 0);
}

int sysbind_yield() {
    return (int)__syscall_eintr(22, 0, 0, 0, 0, 0, 0);
}

int sysbind_getpid() {
    return (int)__syscall_eintr(23, 0, 0, 0, 0, 0, 0);
}

int sysbind_gettid() {
    return (int)__syscall_eintr(24, 0, 0, 0, 0, 0, 0);
}

int sysbind_getuid() {
    return (int)__syscall_eintr(25, 0, 0, 0, 0, 0, 0);
}

int sysbind_geteuid() {
    return (int)__syscall_eintr(26, 0, 0, 0, 0, 0, 0);
}

int sysbind_getgid() {
    return (int)__syscall_eintr(27, 0, 0, 0, 0, 0, 0);
}

int sysbind_getegid() {
    return (int)__syscall_eintr(28, 0, 0, 0, 0, 0, 0);
}

void* sysbind_mmap(void * addr, long length, int prot, int flags, int fd, long offset) {
    return (void*)__syscall_eintr(29, (unsigned long long)addr, (unsigned long long)length, (unsigned long long)prot, (unsigned long long)flags, (unsigned long long)fd, (unsigned long long)offset);
}

int sysbind_munmap(void* addr, unsigned long length) {
    return (int)__syscall_eintr(30, (unsigned long long)addr, (unsigned long long)length, 0, 0, 0, 0);
}

int sysbind_mrename(void * addr, char* name) {
    return (int)__syscall_eintr(31, (unsigned long long)addr, (unsigned long long)name, 0, 0, 0, 0);
}

int sysbind_mgetname(void* addr, char* name, size_t sz) {
    return (int)__syscall_eintr(32, (unsigned long long)addr, (unsigned long long)name, (unsigned long long)sz, 0, 0, 0);
}

int sysbind_mregions(struct mmap_region * regions, int nregions) {
    return (int)__syscall_eintr(33, (unsigned long long)regions, (unsigned long long)nregions, 0, 0, 0, 0);
}

unsigned long sysbind_mshare(int action, void* arg) {
    return (unsigned long)__syscall_eintr(34, (unsigned long long)action, (unsigned long long)arg, 0, 0, 0, 0);
}

int sysbind_dirent(int fd, struct dirent * ent, int offset, int count) {
    return (int)__syscall_eintr(35, (unsigned long long)fd, (unsigned long long)ent, (unsigned long long)offset, (unsigned long long)count, 0, 0);
}

time_t sysbind_localtime(struct tm* tloc) {
    return (time_t)__syscall_eintr(36, (unsigned long long)tloc, 0, 0, 0, 0, 0);
}

size_t sysbind_gettime_microsecond() {
    return (size_t)__syscall_eintr(37, 0, 0, 0, 0, 0, 0);
}

int sysbind_usleep(unsigned long usec) {
    return (int)__syscall_eintr(38, (unsigned long long)usec, 0, 0, 0, 0, 0);
}

int sysbind_socket(int domain, int type, int proto) {
    return (int)__syscall_eintr(39, (unsigned long long)domain, (unsigned long long)type, (unsigned long long)proto, 0, 0, 0);
}

ssize_t sysbind_sendto(int sockfd, const void * buf, size_t len, int flags, const struct sockaddr* addr, size_t addrlen) {
    return (ssize_t)__syscall_eintr(40, (unsigned long long)sockfd, (unsigned long long)buf, (unsigned long long)len, (unsigned long long)flags, (unsigned long long)addr, (unsigned long long)addrlen);
}

ssize_t sysbind_recvfrom(int sockfd, void * buf, size_t len, int flags, const struct sockaddr* addr, size_t addrlen) {
    return (ssize_t)__syscall_eintr(41, (unsigned long long)sockfd, (unsigned long long)buf, (unsigned long long)len, (unsigned long long)flags, (unsigned long long)addr, (unsigned long long)addrlen);
}

int sysbind_bind(int sockfd, const struct sockaddr* addr, size_t addrlen) {
    return (int)__syscall_eintr(42, (unsigned long long)sockfd, (unsigned long long)addr, (unsigned long long)addrlen, 0, 0, 0);
}

int sysbind_accept(int sockfd, struct sockaddr* addr, size_t addrlen) {
    return (int)__syscall_eintr(43, (unsigned long long)sockfd, (unsigned long long)addr, (unsigned long long)addrlen, 0, 0, 0);
}

int sysbind_connect(int sockfd, const struct sockaddr* addr, size_t addrlen) {
    return (int)__syscall_eintr(44, (unsigned long long)sockfd, (unsigned long long)addr, (unsigned long long)addrlen, 0, 0, 0);
}

int sysbind_signal_init(void * sigret) {
    return (int)__syscall_eintr(45, (unsigned long long)sigret, 0, 0, 0, 0, 0);
}

int sysbind_sigaction(int sig, struct sigaction* new_action, struct sigaction* old) {
    return (int)__syscall_eintr(46, (unsigned long long)sig, (unsigned long long)new_action, (unsigned long long)old, 0, 0, 0);
}

int sysbind_sigreturn() {
    return (int)__syscall_eintr(47, 0, 0, 0, 0, 0, 0);
}

int sysbind_sigprocmask(int how, unsigned long set, unsigned long* old_set) {
    return (int)__syscall_eintr(48, (unsigned long long)how, (unsigned long long)set, (unsigned long long)old_set, 0, 0, 0);
}

int sysbind_awaitfs(struct await_target * fds, int nfds, int flags, long long timeout_time) {
    return (int)__syscall_eintr(49, (unsigned long long)fds, (unsigned long long)nfds, (unsigned long long)flags, (unsigned long long)timeout_time, 0, 0);
}

unsigned long sysbind_kshell(char* cmd, int argc, char ** argv, void* data, size_t len) {
    return (unsigned long)__syscall_eintr(50, (unsigned long long)cmd, (unsigned long long)argc, (unsigned long long)argv, (unsigned long long)data, (unsigned long long)len, 0);
}

int sysbind_futex(int* uaddr, int op, int val, int val2, int* uaddr2, int val3) {
    return (int)__syscall_eintr(51, (unsigned long long)uaddr, (unsigned long long)op, (unsigned long long)val, (unsigned long long)val2, (unsigned long long)uaddr2, (unsigned long long)val3);
}

