#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/sysbind.h>
#include <stdint.h>

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "usage: %s [names...]\n", argv[0]);
    return EXIT_FAILURE;
  }

  char buf[32];
  for (int i = 1; i < argc; i++) {
    uint32_t ip4 = 0;
    if (sysbind_dnslookup(argv[i], &ip4) == 0) {
      inet_ntop(AF_INET, &ip4, buf, 32);
      printf("%s: %s\n", argv[i], buf);
    }
  }
  return EXIT_SUCCESS;
}
