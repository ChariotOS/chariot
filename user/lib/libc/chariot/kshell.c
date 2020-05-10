#include <sys/kshell.h>
#include <sys/syscall.h>


unsigned long kshell(const char *command, int argc, char **argv, void *data,
		     unsigned long len) {
	return errno_syscall(SYS_kshell, command, argc, argv, data, len);
}
