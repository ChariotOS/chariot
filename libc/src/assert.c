#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void __assert_fail(const char* assertion, const char* file, unsigned int line,
                   const char* function) {
  fprintf(stderr, "Assertion failed: (%s in %s:%d) '%s'\n", function, file, line, assertion);
  exit(EXIT_FAILURE);
}
