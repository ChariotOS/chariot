#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENT_PORT 68
#define SERVER_PORT 67

#define DHCP_MAGIC 0x63825363
struct dhcp_hdr {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;

  uint32_t xid;

  uint16_t secs;
  uint16_t flags;

  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t siaddr;
  uint32_t giaddr;

  uint8_t chaddr[16];

  uint8_t sname[64];
  uint8_t file[128];

  uint32_t magic;

  uint8_t options[];
} __attribute__((packed));

int dhcp_setup(int sk, struct sockaddr_in *a) {
  struct dhcp_hdr d;

  d.magic = htonl(DHCP_MAGIC);

  int n = sendto(sk, &d, sizeof(d), 0, (struct sockaddr *)a, sizeof(*a));
  if (n < 0) {
    perror("dhcp_setup send");
    exit(-1);
  }
  return 0;
}

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
  struct sockaddr_in bindaddr = mksockaddr("0.0.0.0", CLIENT_PORT);

  int r = bind(sk, (struct sockaddr *)&bindaddr, sizeof(bindaddr));
  if (r != 0) {
    int e = errno;
    printf("e: %d, r: %d\n", e, r);
    exit(-1);
  }
  dhcp_setup(sk, &servaddr);
  /*
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
  */

  close(sk);

  return 0;
}
