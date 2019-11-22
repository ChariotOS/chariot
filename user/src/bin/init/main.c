#include <unistd.h>
#include <string.h>
#include <chariot.h>
#include <stdio.h>


int main(int argc, char **argv) {
  printf("hello, world %d %p\n", argc, argv);
  while (1) {
    // from chariot.h :)
    yield();
  }
}







