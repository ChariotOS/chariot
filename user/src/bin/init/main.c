#include <unistd.h>
#include <string.h>


int main(int argc, char **argv) {
  char *msg = "Hello, World\n";
  write(0, msg, strlen(msg));
  while (1) {
  }
}
