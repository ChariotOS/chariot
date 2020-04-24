#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define CLIENT_PORT 68
#define SERVER_PORT 67



struct sockaddr_in mksockaddr(const char *ip, uint16_t port) {

  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = inet_addr(ip);

	return a;
}

int main(int argc, char **argv) {
  int sk = socket(AF_INET, SOCK_DGRAM, 0);

  if (sk == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in servaddr = mksockaddr("255.255.255.255", SERVER_PORT);
  struct sockaddr_in bindaddr = mksockaddr("255.255.255.255", CLIENT_PORT);

	int r = bind(sk, (struct sockaddr *)&bindaddr, sizeof(bindaddr));
  if (r != 0) {
		int e = errno;
		printf("e: %d, r: %d\n", e, r);
    exit(-1);
  }

  for (int i = 0; i < 1; i++) {
    char buf[50];
    sprintf(buf, "msg %d\n", i);
    int n = sendto(sk, buf, strlen(buf) + 1, 0,
		   (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (n < 0) {
      perror("sendto");
      close(sk);
      exit(-1);
    }
  }

  close(sk);

  return 0;
}
