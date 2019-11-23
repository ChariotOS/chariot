#include <chariot.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define BUFSZ 50000
int buf[BUFSZ];

int main(int argc, char **argv) {
  printf("hello, world\n");

  void *ptr =
      mmap(NULL, 4096 * 800, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

  if (ptr == MAP_FAILED) {
    printf("failed to map!\n");
  } else {
    printf("mapped region: %p\n", ptr);
    strcpy(ptr, "hello\n");
    printf("in region: '%s'\n", ptr);
  }

  while (1) {
    yield();
  }

  int c = 0;
  while (1) {
    memset(buf, c, sizeof(buf) / sizeof(buf[0]));
    uint64_t sum = 0;
    for (int i = 0; i < BUFSZ; i++) {
      sum += buf[i];
    }
    // printf("%02x: %016llx\n", c, sum);

    c++;
    c &= 0xFF;
  }
  while (1) {
    // from chariot.h :)
    yield();
  }
}

