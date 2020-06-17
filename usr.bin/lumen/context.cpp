#include <errno.h>
#include <string.h>
#include "internal.h"
#include <lumen/msg.h>

lumen::context::context(void) : screen(1024, 768) {
  // clear the screen (black)
  memset(screen.pixels(), 0x00, screen.screensize());

  keyboard.open("/dev/keyboard", "r+");
  keyboard.on_read([this] {
    while (1) {
      keyboard_packet_t pkt;
      keyboard.read(&pkt, sizeof(pkt));
      if (errno == EAGAIN) break;
      handle_keyboard_input(pkt);
    }
  });


  mouse.open("/dev/mouse", "r+");
  mouse.on_read([this] {
    while (1) {
      struct mouse_packet pkt;
      mouse.read(&pkt, sizeof(pkt));
      if (errno == EAGAIN) break;
      handle_mouse_input(pkt);
      // printf("mouse: dx: %-3d dy: %-3d buttons: %02x\n", pkt.dx, pkt.dy,
      // pkt.buttons);
    }
  });


  server.listen("/usr/servers/lumen", [this] { accept_connection(); });
}


void lumen::context::handle_keyboard_input(keyboard_packet_t &pkt) {
  printf("keyboard: code: %02x ch: %02x (%c)\n", pkt.key, pkt.character,
         pkt.character);
}

void lumen::context::handle_mouse_input(struct mouse_packet &pkt) {
  printf("mouse: dx: %-3d dy: %-3d buttons: %02x\n", pkt.dx, pkt.dy,
         pkt.buttons);
}


static ck::vec<lumen::msg *> drain_messages(ck::localsocket &sock,
                                            bool &failed) {
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

void lumen::context::accept_connection() {
  auto id = next_client_id++;
  // accept the connection
  auto *client = server.accept();
  clients.set(id, client);

  printf("got a connection\n");

  client->on_read([id, client, this] {
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
}
