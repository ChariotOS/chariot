#include <stdio.h>
#include <sys/syscall.h>

int main() {
  char *cwd[255];
  syscall(SYS_getcwd, cwd, 255);
  puts((const char *)cwd);
  return 0;
}
