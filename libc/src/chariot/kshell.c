#include <sys/kshell.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>

unsigned long kshell(const char *command, int argc, char **argv, void *data, unsigned long len) {
  return errno_wrap(sysbind_kshell((char *)command, argc, argv, data, len));
}
