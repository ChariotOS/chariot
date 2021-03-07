#include <sys/sysbind.h>
#include <stdio.h>

int main() {
  sysbind_shutdown();
  perror("Couldn't shutdown");
  return 0;
}
