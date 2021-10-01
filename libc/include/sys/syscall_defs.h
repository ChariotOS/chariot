#define SYS_restart                  (0x00)
#define SYS_exit_thread              (0x01)
#define SYS_exit_proc                (0x02)
#define SYS_execve                   (0x03)
#define SYS_waitpid                  (0x04)
#define SYS_fork                     (0x05)
#define SYS_spawnthread              (0x06)
#define SYS_jointhread               (0x07)
#define SYS_sigwait                  (0x08)
#define SYS_prctl                    (0x09)
#define SYS_open                     (0x0a)
#define SYS_close                    (0x0b)
#define SYS_lseek                    (0x0c)
#define SYS_read                     (0x0d)
#define SYS_write                    (0x0e)
#define SYS_stat                     (0x0f)
#define SYS_fstat                    (0x10)
#define SYS_lstat                    (0x11)
#define SYS_dup                      (0x12)
#define SYS_dup2                     (0x13)
#define SYS_mkdir                    (0x14)
#define SYS_chdir                    (0x15)
#define SYS_getcwd                   (0x16)
#define SYS_chroot                   (0x17)
#define SYS_unlink                   (0x18)
#define SYS_ioctl                    (0x19)
#define SYS_yield                    (0x1a)
#define SYS_getpid                   (0x1b)
#define SYS_gettid                   (0x1c)
#define SYS_getuid                   (0x1d)
#define SYS_geteuid                  (0x1e)
#define SYS_getgid                   (0x1f)
#define SYS_getegid                  (0x20)
#define SYS_setpgid                  (0x21)
#define SYS_getpgid                  (0x22)
#define SYS_mmap                     (0x23)
#define SYS_munmap                   (0x24)
#define SYS_mrename                  (0x25)
#define SYS_mgetname                 (0x26)
#define SYS_mregions                 (0x27)
#define SYS_mshare                   (0x28)
#define SYS_dirent                   (0x29)
#define SYS_localtime                (0x2a)
#define SYS_gettime_microsecond      (0x2b)
#define SYS_usleep                   (0x2c)
#define SYS_socket                   (0x2d)
#define SYS_sendto                   (0x2e)
#define SYS_recvfrom                 (0x2f)
#define SYS_bind                     (0x30)
#define SYS_accept                   (0x31)
#define SYS_connect                  (0x32)
#define SYS_signal_init              (0x33)
#define SYS_sigaction                (0x34)
#define SYS_sigreturn                (0x35)
#define SYS_sigprocmask              (0x36)
#define SYS_kill                     (0x37)
#define SYS_awaitfs                  (0x38)
#define SYS_kshell                   (0x39)
#define SYS_futex                    (0x3a)
#define SYS_sysinfo                  (0x3b)
#define SYS_dnslookup                (0x3c)
#define SYS_shutdown                 (0x3d)
#define SYS_mount                    (0x3e)
#define SYS_getraminfo               (0x3f)
#define SYS_get_core_usage           (0x40)
#define SYS_get_nproc                (0x41)