#include <stdio.h>
#include <sys/sysbind.h>

int main() {
  char *cwd[255];
  sysbind_getcwd((char*)cwd, 255);
  puts((const char *)cwd);
  return 0;
}
