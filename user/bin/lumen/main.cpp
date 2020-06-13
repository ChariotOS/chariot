#include <ck/io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <lumen/msg.h>

#include "internal.h"


char upper(char ch) {
  if (ch >= 'a' && ch <= 'z')
    return ('A' + ch - 'a');
  else
    return ch;
}



struct lumen::msg *read_msg(int fd, int &err) {
  // read in the base data
  struct lumen::msg base;
  int n = read(fd, &base, sizeof(base));
  if (n < 0) {
    err = n;
    return NULL;
  }
  // TODO: this could break stuff!
  if (n <= 0) return NULL;

  auto *msg = (lumen::msg *)malloc(sizeof(lumen::msg) + base.len);
  msg->type = base.type;
  msg->id = base.id;
  msg->len = base.len;


  if (base.len > 0) {
    int n = read(fd, msg + 1, msg->len);
    if (n < 0) {
      free(msg);
      err = n;
      return NULL;
    }
  }
	err = 0;
  return msg;
}



int main(int argc, char **argv) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  // bind the local socket to the windowserver
  strncpy(addr.sun_path, "/usr/servers/lumen", sizeof(addr.sun_path) - 1);
  int bind_res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (bind_res < 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // server
  while (1) {
    int client = accept(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (client < 0) continue;

    printf("[server] client connected %d\n", client);

    while (1) {
      int err = 0;
      auto *msg = read_msg(client, err);
      // printf("msg: %p, err: %d\n", msg, err);
      if (err != 0) break;

			if (msg != NULL) {
				printf("id: %3lu, len: %3d, type: %3d\n", msg->id, msg->len, msg->type);
				ck::hexdump(&msg->data, msg->len);
			}

      free(msg);
    }
    printf("[server] client disconnected\n");

    close(client);
  }

  return 0;
}
