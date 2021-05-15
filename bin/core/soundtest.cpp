#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

int main() {
  uint16_t *buffer = (uint16_t *)malloc(4096);

  for (int i = 0; i < 4096 / sizeof(uint16_t); i++) {
    buffer[i] = rand();
  }


  int fd = open("/dev/sb16", O_WRONLY);

  printf("%d\n", fd);

  write(fd, buffer, 4096);



  return 0;
}
