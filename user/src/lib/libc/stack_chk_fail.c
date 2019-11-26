#include <stdio.h>

void __stack_chk_fail(void) {
  printf("stack check failed!\n");
  while (1) {} // TODO: crash!
}

