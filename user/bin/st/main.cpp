#include <ck/io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


char upper(char ch) {
  if (ch >= 'a' && ch <= 'z')
    return ('A' + ch - 'a');
  else
    return ch;
}

int main(int argc, char **argv) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  // bind the local socket to the windowserver
  strncpy(addr.sun_path, "/usr/servers/windowserver",
          sizeof(addr.sun_path) - 1);
  int bind_res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (bind_res < 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

	// server
  while (1) {
    int client = accept(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (client < 0) continue;

		int len = 0;
		read(client, &len, sizeof(len));

		auto *buf = (char*)malloc(len);

    int n = read(client, buf, len);

    for (int i = 0; i < n; i++) {
      buf[i] = upper(buf[i]);
    }

    write(client, buf, n);
		free(buf);


    close(client);
  }

  return 0;
}
