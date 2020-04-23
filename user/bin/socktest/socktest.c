#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
  struct sockaddr_in servaddr;
  int sk = socket(AF_INET, SOCK_DGRAM, 0);

  if (sk == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(2115);
  servaddr.sin_addr.s_addr = inet_addr("8.8.8.8");
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
