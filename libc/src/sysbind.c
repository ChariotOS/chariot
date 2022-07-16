#include <sys/sysbind.h>
#include <sys/syscall.h>

extern long __syscall_eintr(int num, unsigned long long a, unsigned long long b,
                      unsigned long long c, unsigned long long d,
                      unsigned long long e, unsigned long long f);void sysbind_restart() {
    return (void)__syscall_eintr(SYS_restart,
               0,
               0,
               0,
               0,
               0,
               0);
}

void sysbind_exit_thread(int code) {
    return (void)__syscall_eintr(SYS_exit_thread,
               (unsigned long long)code,
               0,
               0,
               0,
               0,
               0);
}

void sysbind_exit_proc(int code) {
    return (void)__syscall_eintr(SYS_exit_proc,
               (unsigned long long)code,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_execve(const char* path, const char ** argv, const char ** envp) {
    return (int)__syscall_eintr(SYS_execve,
               (unsigned long long)path,
               (unsigned long long)argv,
               (unsigned long long)envp,
               0,
               0,
               0);
}

long sysbind_waitpid(int pid, int* stat, int options) {
    return (long)__syscall_eintr(SYS_waitpid,
               (unsigned long long)pid,
               (unsigned long long)stat,
               (unsigned long long)options,
               0,
               0,
               0);
}

int sysbind_fork() {
    return (int)__syscall_eintr(SYS_fork,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_spawnthread(void * stack, void* func, void* arg, int flags) {
    return (int)__syscall_eintr(SYS_spawnthread,
               (unsigned long long)stack,
               (unsigned long long)func,
               (unsigned long long)arg,
               (unsigned long long)flags,
               0,
               0);
}

int sysbind_jointhread(int tid) {
    return (int)__syscall_eintr(SYS_jointhread,
               (unsigned long long)tid,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_sigwait() {
    return (int)__syscall_eintr(SYS_sigwait,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_prctl(int option, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    return (int)__syscall_eintr(SYS_prctl,
               (unsigned long long)option,
               (unsigned long long)arg1,
               (unsigned long long)arg2,
               (unsigned long long)arg3,
               (unsigned long long)arg4,
               (unsigned long long)arg5);
}

int sysbind_open(const char * path, int flags, int mode) {
    return (int)__syscall_eintr(SYS_open,
               (unsigned long long)path,
               (unsigned long long)flags,
               (unsigned long long)mode,
               0,
               0,
               0);
}

int sysbind_close(int fd) {
    return (int)__syscall_eintr(SYS_close,
               (unsigned long long)fd,
               0,
               0,
               0,
               0,
               0);
}

long sysbind_lseek(int fd, long offset, int whence) {
    return (long)__syscall_eintr(SYS_lseek,
               (unsigned long long)fd,
               (unsigned long long)offset,
               (unsigned long long)whence,
               0,
               0,
               0);
}

long sysbind_read(int fd, void* buf, size_t len) {
    return (long)__syscall_eintr(SYS_read,
               (unsigned long long)fd,
               (unsigned long long)buf,
               (unsigned long long)len,
               0,
               0,
               0);
}

long sysbind_write(int fd, void* buf, size_t len) {
    return (long)__syscall_eintr(SYS_write,
               (unsigned long long)fd,
               (unsigned long long)buf,
               (unsigned long long)len,
               0,
               0,
               0);
}

int sysbind_stat(const char* pathname, struct stat* statbuf) {
    return (int)__syscall_eintr(SYS_stat,
               (unsigned long long)pathname,
               (unsigned long long)statbuf,
               0,
               0,
               0,
               0);
}

int sysbind_fstat(int fd, struct stat* statbuf) {
    return (int)__syscall_eintr(SYS_fstat,
               (unsigned long long)fd,
               (unsigned long long)statbuf,
               0,
               0,
               0,
               0);
}

int sysbind_lstat(const char* pathname, struct stat* statbuf) {
    return (int)__syscall_eintr(SYS_lstat,
               (unsigned long long)pathname,
               (unsigned long long)statbuf,
               0,
               0,
               0,
               0);
}

int sysbind_dup(int fd) {
    return (int)__syscall_eintr(SYS_dup,
               (unsigned long long)fd,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_dup2(int old, int newfd) {
    return (int)__syscall_eintr(SYS_dup2,
               (unsigned long long)old,
               (unsigned long long)newfd,
               0,
               0,
               0,
               0);
}

int sysbind_mkdir(const char* path, int mode) {
    return (int)__syscall_eintr(SYS_mkdir,
               (unsigned long long)path,
               (unsigned long long)mode,
               0,
               0,
               0,
               0);
}

int sysbind_chdir(const char* path) {
    return (int)__syscall_eintr(SYS_chdir,
               (unsigned long long)path,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_getcwd(char * dst, int len) {
    return (int)__syscall_eintr(SYS_getcwd,
               (unsigned long long)dst,
               (unsigned long long)len,
               0,
               0,
               0,
               0);
}

int sysbind_chroot(const char * path) {
    return (int)__syscall_eintr(SYS_chroot,
               (unsigned long long)path,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_unlink(const char * path) {
    return (int)__syscall_eintr(SYS_unlink,
               (unsigned long long)path,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_ioctl(int fd, int cmd, unsigned long value) {
    return (int)__syscall_eintr(SYS_ioctl,
               (unsigned long long)fd,
               (unsigned long long)cmd,
               (unsigned long long)value,
               0,
               0,
               0);
}

int sysbind_yield() {
    return (int)__syscall_eintr(SYS_yield,
               0,
               0,
               0,
               0,
               0,
               0);
}

long sysbind_getpid() {
    return (long)__syscall_eintr(SYS_getpid,
               0,
               0,
               0,
               0,
               0,
               0);
}

long sysbind_gettid() {
    return (long)__syscall_eintr(SYS_gettid,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_getuid() {
    return (int)__syscall_eintr(SYS_getuid,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_geteuid() {
    return (int)__syscall_eintr(SYS_geteuid,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_getgid() {
    return (int)__syscall_eintr(SYS_getgid,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_getegid() {
    return (int)__syscall_eintr(SYS_getegid,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_setpgid(int pid, int pgid) {
    return (int)__syscall_eintr(SYS_setpgid,
               (unsigned long long)pid,
               (unsigned long long)pgid,
               0,
               0,
               0,
               0);
}

int sysbind_getpgid(int pid) {
    return (int)__syscall_eintr(SYS_getpgid,
               (unsigned long long)pid,
               0,
               0,
               0,
               0,
               0);
}

void* sysbind_mmap(void * addr, long length, int prot, int flags, int fd, long offset) {
    return (void*)__syscall_eintr(SYS_mmap,
               (unsigned long long)addr,
               (unsigned long long)length,
               (unsigned long long)prot,
               (unsigned long long)flags,
               (unsigned long long)fd,
               (unsigned long long)offset);
}

int sysbind_munmap(void* addr, size_t length) {
    return (int)__syscall_eintr(SYS_munmap,
               (unsigned long long)addr,
               (unsigned long long)length,
               0,
               0,
               0,
               0);
}

int sysbind_mrename(void * addr, char* name) {
    return (int)__syscall_eintr(SYS_mrename,
               (unsigned long long)addr,
               (unsigned long long)name,
               0,
               0,
               0,
               0);
}

int sysbind_mgetname(void* addr, char* name, size_t sz) {
    return (int)__syscall_eintr(SYS_mgetname,
               (unsigned long long)addr,
               (unsigned long long)name,
               (unsigned long long)sz,
               0,
               0,
               0);
}

int sysbind_mregions(struct mmap_region * regions, int nregions) {
    return (int)__syscall_eintr(SYS_mregions,
               (unsigned long long)regions,
               (unsigned long long)nregions,
               0,
               0,
               0,
               0);
}

unsigned long sysbind_mshare(int action, void* arg) {
    return (unsigned long)__syscall_eintr(SYS_mshare,
               (unsigned long long)action,
               (unsigned long long)arg,
               0,
               0,
               0,
               0);
}

int sysbind_dirent(int fd, struct dirent * ent, int offset, int count) {
    return (int)__syscall_eintr(SYS_dirent,
               (unsigned long long)fd,
               (unsigned long long)ent,
               (unsigned long long)offset,
               (unsigned long long)count,
               0,
               0);
}

time_t sysbind_localtime(struct tm* tloc) {
    return (time_t)__syscall_eintr(SYS_localtime,
               (unsigned long long)tloc,
               0,
               0,
               0,
               0,
               0);
}

size_t sysbind_gettime_microsecond() {
    return (size_t)__syscall_eintr(SYS_gettime_microsecond,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_usleep(unsigned long usec) {
    return (int)__syscall_eintr(SYS_usleep,
               (unsigned long long)usec,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_socket(int domain, int type, int proto) {
    return (int)__syscall_eintr(SYS_socket,
               (unsigned long long)domain,
               (unsigned long long)type,
               (unsigned long long)proto,
               0,
               0,
               0);
}

ssize_t sysbind_sendto(int sockfd, const void * buf, size_t len, int flags, const struct sockaddr* addr, size_t addrlen) {
    return (ssize_t)__syscall_eintr(SYS_sendto,
               (unsigned long long)sockfd,
               (unsigned long long)buf,
               (unsigned long long)len,
               (unsigned long long)flags,
               (unsigned long long)addr,
               (unsigned long long)addrlen);
}

ssize_t sysbind_recvfrom(int sockfd, void * buf, size_t len, int flags, const struct sockaddr* addr, size_t addrlen) {
    return (ssize_t)__syscall_eintr(SYS_recvfrom,
               (unsigned long long)sockfd,
               (unsigned long long)buf,
               (unsigned long long)len,
               (unsigned long long)flags,
               (unsigned long long)addr,
               (unsigned long long)addrlen);
}

int sysbind_bind(int sockfd, const struct sockaddr* addr, size_t addrlen) {
    return (int)__syscall_eintr(SYS_bind,
               (unsigned long long)sockfd,
               (unsigned long long)addr,
               (unsigned long long)addrlen,
               0,
               0,
               0);
}

int sysbind_accept(int sockfd, struct sockaddr* addr, size_t addrlen) {
    return (int)__syscall_eintr(SYS_accept,
               (unsigned long long)sockfd,
               (unsigned long long)addr,
               (unsigned long long)addrlen,
               0,
               0,
               0);
}

int sysbind_connect(int sockfd, const struct sockaddr* addr, size_t addrlen) {
    return (int)__syscall_eintr(SYS_connect,
               (unsigned long long)sockfd,
               (unsigned long long)addr,
               (unsigned long long)addrlen,
               0,
               0,
               0);
}

int sysbind_signal_init(void * sigret) {
    return (int)__syscall_eintr(SYS_signal_init,
               (unsigned long long)sigret,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_sigaction(int sig, struct sigaction* new_action, struct sigaction* old) {
    return (int)__syscall_eintr(SYS_sigaction,
               (unsigned long long)sig,
               (unsigned long long)new_action,
               (unsigned long long)old,
               0,
               0,
               0);
}

int sysbind_sigreturn(void* ucontext) {
    return (int)__syscall_eintr(SYS_sigreturn,
               (unsigned long long)ucontext,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_sigprocmask(int how, unsigned long set, unsigned long* old_set) {
    return (int)__syscall_eintr(SYS_sigprocmask,
               (unsigned long long)how,
               (unsigned long long)set,
               (unsigned long long)old_set,
               0,
               0,
               0);
}

int sysbind_kill(int pid, int sig) {
    return (int)__syscall_eintr(SYS_kill,
               (unsigned long long)pid,
               (unsigned long long)sig,
               0,
               0,
               0,
               0);
}

int sysbind_awaitfs(struct await_target * fds, int nfds, int flags, long long timeout_time) {
    return (int)__syscall_eintr(SYS_awaitfs,
               (unsigned long long)fds,
               (unsigned long long)nfds,
               (unsigned long long)flags,
               (unsigned long long)timeout_time,
               0,
               0);
}

unsigned long sysbind_kshell() {
    return (unsigned long)__syscall_eintr(SYS_kshell,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_futex(int* uaddr, int op, int val, int val2, int* uaddr2, int val3) {
    return (int)__syscall_eintr(SYS_futex,
               (unsigned long long)uaddr,
               (unsigned long long)op,
               (unsigned long long)val,
               (unsigned long long)val2,
               (unsigned long long)uaddr2,
               (unsigned long long)val3);
}

int sysbind_sysinfo(struct sysinfo * info) {
    return (int)__syscall_eintr(SYS_sysinfo,
               (unsigned long long)info,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_dnslookup(const char * name, unsigned int* ip4) {
    return (int)__syscall_eintr(SYS_dnslookup,
               (unsigned long long)name,
               (unsigned long long)ip4,
               0,
               0,
               0,
               0);
}

int sysbind_shutdown() {
    return (int)__syscall_eintr(SYS_shutdown,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_mount(struct mountopts * opts) {
    return (int)__syscall_eintr(SYS_mount,
               (unsigned long long)opts,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_getraminfo(unsigned long long * avail, unsigned long long * total) {
    return (int)__syscall_eintr(SYS_getraminfo,
               (unsigned long long)avail,
               (unsigned long long)total,
               0,
               0,
               0,
               0);
}

unsigned long sysbind_getramusage() {
    return (unsigned long)__syscall_eintr(SYS_getramusage,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_get_core_usage(unsigned int core, struct chariot_core_usage * usage) {
    return (int)__syscall_eintr(SYS_get_core_usage,
               (unsigned long long)core,
               (unsigned long long)usage,
               0,
               0,
               0,
               0);
}

int sysbind_get_nproc() {
    return (int)__syscall_eintr(SYS_get_nproc,
               0,
               0,
               0,
               0,
               0,
               0);
}

int sysbind_kctl(off_t* name, unsigned namelen, char * oval, size_t* olen, char * nval, size_t nlen) {
    return (int)__syscall_eintr(SYS_kctl,
               (unsigned long long)name,
               (unsigned long long)namelen,
               (unsigned long long)oval,
               (unsigned long long)olen,
               (unsigned long long)nval,
               (unsigned long long)nlen);
}

