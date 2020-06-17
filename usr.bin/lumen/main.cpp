#include <chariot/keycode.h>
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

ck::vec<lumen::msg *> drain_messages(ck::localsocket &sock, bool &failed) {
  failed = false;
  ck::vec<uint8_t> bytes;
  for (;;) {
    uint8_t buffer[512];  // XXX dont put this on the stack
    int nread = sock.recv(buffer, sizeof(buffer), MSG_DONTWAIT);
    int errno_cache = errno;
    if (nread < 0) {
      if (errno_cache == EAGAIN) break;
      failed = true;
      return {};
    }

    if (nread == 0) {
      failed = true;
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

  failed = false;

  return msgs;
}


static ck::map<long, ck::localsocket *> clients;
static int next_id = 0;

int main(int argc, char **argv) {
  ck::eventloop loop;
  ck::localsocket server;

  ck::file keyboard("/dev/keyboard", "r+");

  keyboard.on_read([&] {
    while (1) {
      keyboard_packet_t pkt;
      keyboard.read(&pkt, sizeof(pkt));
      if (errno == EAGAIN) break;
      ck::hexdump(&pkt, sizeof(pkt));
    }
  });

  server.listen("/usr/servers/lumen", [&] {
    auto id = next_id++;
    // accept the connection
    auto *client = server.accept();
    clients.set(id, client);

    printf("got a connection\n");

    client->on_read([id, client] {
      printf("here\n");
      bool failed = false;
      auto msgs = drain_messages(*client, failed);
      // handle messages

      for (auto *msg : msgs) {
        ck::hexdump(msg, sizeof(*msg) + msg->len);
        free(msg);
      }

      // if the client is at EOF *or* it otherwise failed, consider it
      // disconnected
      if (client->eof() || failed) {
        printf("Client %d disconnected\n", id);
        clients.remove(id);
        delete client;
      }
    });
  });

  loop.start();

  return 0;
}
