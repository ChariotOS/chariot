#include <ck/eventloop.h>
#include <ck/func.h>
#include <ck/io.h>
#include <ck/map.h>
#include <ck/socket.h>
#include <ck/vec.h>
#include <errno.h>
#include <fcntl.h>
#include <lumen/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "internal.h"


ck::vec<lumen::msg *> drain_messages(ck::localsocket &sock, int &err) {
  ck::vec<uint8_t> bytes;
  for (;;) {
    uint8_t buffer[512];  // XXX dont put this on the stack
    int nread = sock.recv(buffer, sizeof(buffer), MSG_DONTWAIT);
    if (nread == -EAGAIN) {
      break;
    }

    if (nread <= 0) {
      err = nread;
      return {};
    }
    if (nread > 0) {
      bytes.push(buffer, nread);
    }
  }

  ck::vec<lumen::msg *> msgs;

  uint8_t *data = bytes.data();
  for (int i = 0; i < bytes.size();) {
    auto *m = (lumen::msg *)(data + i);
    // if it isn't the magic number, we gotta walk to the next
    // magic number
    if (m->magic != LUMEN_MAGIC) {
      i += 1;
      fprintf(stderr, "LUMEN_MAGIC is not correct\n");
      continue;
    }

    auto total_size = sizeof(lumen::msg) + m->len;
    if (i + total_size > (size_t)bytes.size()) {
      fprintf(stderr, "malformed!\n");
      break;
    }

    auto msg = (lumen::msg *)malloc(total_size);
    memcpy(msg, data + i, total_size);

    msgs.push(msg);
    i += total_size;
  }

  return msgs;
}


int main(int argc, char **argv) {

  ck::eventloop loop;
  ck::localsocket server;

  ck::vec<ck::ref<ck::localsocket>> clients;

  server.bind("/usr/servers/lumen");

  server.on_read = [&] {
    auto client_ref = server.accept();
		auto *client = client_ref.get();
		printf("connected to %p\n", client);
    clients.push(client_ref);

    client->on_read = [=] {
      int err = 0;
      auto msgs = drain_messages(*client, err);
      if (err < 0) {
        fprintf(stderr, "goodbye\n");
				// TODO: close the client
      }
			// do stuff with the messages

      for (auto *msg : msgs) {
        printf("%c", msg->data[0]);
        free(msg);
      }
      fflush(stdout);
    };
  };

  loop.start();

  return 0;
}
