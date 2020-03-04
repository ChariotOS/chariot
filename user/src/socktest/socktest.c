#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int sk = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in servaddr;
  if (sk == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }
  /*
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);
  servaddr.sin_addr.s_addr = INADDR_ANY;
  */
  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_addr.s_addr = inet_addr("192.168.0.1");
  unsigned int ip = inet_addr("192.186.0.1");
  printf("ip=%08x\n", ip);
  printf("ip=%s\n", inet_ntoa(servaddr.sin_addr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(6000);
  servaddr.sin_addr.s_addr = INADDR_ANY;
  const char *msg = "hello";
  write(sk, msg, strlen(msg) + 1);
  close(sk);

  return 0;
}
