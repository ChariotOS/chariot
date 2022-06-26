#include <err.h>
#include <stdio.h>
#include <unistd.h>


// Basically stolen from openbsd
int main(int argc, char *argv[]) {
  if (argc > 1)
    for (;;)
      puts(argv[1]);
  else
    for (;;)
      puts("y");
}
