#include <errno.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <string.h>
#include "internal.h"

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



void lumen::context::accept_connection() {
  auto id = next_client_id++;
  // accept the connection
  auto *client = server.accept();
  clients.set(id, new lumen::client(id, *this, client));
}

void lumen::context::client_closed(long id) {
  auto c = clients[id];
  clients.remove(id);
  delete c;
}


#define HANDLE_TYPE(t, data_type)       \
  if (auto arg = (data_type *)msg.data; \
      msg.type == t && msg.len == sizeof(data_type))
void lumen::context::process_message(lumen::client &c, lumen::msg &msg) {
  HANDLE_TYPE(LUMEN_MSG_CREATE_WINDOW, lumen::create_window_msg) {
    (void)arg;

    ck::string name(arg->name, LUMEN_NAMESZ);

		/*
    printf("window wants to be made! ('%s', %dx%d)\n", name.get(), arg->width,
           arg->height);
					 */

    struct lumen::window_created_msg res;
		// TODO: figure out a better position to open to
		auto *window = c.new_window(name, gfx::rect(arg->width, arg->height, 0, 0));
		res.window_id = window->id;

    c.respond(msg, LUMEN_MSG_WINDOW_CREATED, res);
    return;
  }

  HANDLE_TYPE(LUMEN_MSG_GREET, lumen::greet_msg) {
    (void)arg;
    // responed to that
    struct lumen::greetback_msg res;
    res.magic = LUMEN_GREETBACK_MAGIC;
    res.client_id = c.id;
    c.respond(msg, msg.type, res);
  };
}


lumen::client::client(long id, struct context &ctx, ck::localsocket *conn)
    : id(id), ctx(ctx), connection(conn) {
  // printf("got a connection\n");

  connection->on_read([this] { this->on_read(); });
}

lumen::client::~client(void) {
  for (auto kv : windows) {
    printf("window '%s' (id: %d) removed!\n", kv.value->name.get(), kv.key);
    // XXX: notify the context of the window destructions
    delete kv.value;
  }
  delete connection;
}

void lumen::client::on_read(void) {
  bool failed = false;
  auto msgs = lumen::drain_messages(*connection, failed);
  // handle messages

  for (auto *msg : msgs) {
    process_message(*msg);
    free(msg);
  }

  // if the client is at EOF *or* it otherwise failed, consider it
  // disconnected
  if (connection->eof() || failed) {
    this->ctx.client_closed(id);
  }
}



struct lumen::window *lumen::client::new_window(ck::string name, gfx::rect r) {
	auto id = next_window_id++;
	auto w = new lumen::window(id, *this);
	w->name = name;
	w->rect = r;
	printf("client %d made a new window, '%s'\n", this->id, name.get());
	windows.set(w->id, w);
	return w;
}

void lumen::client::process_message(lumen::msg &msg) {
  // defer to the window server's function
  ctx.process_message(*this, msg);
}


long lumen::client::send_raw(int type, int id, void *payload,
                             size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = id;
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy(msg + 1, payload, payloadsize);

  auto w = connection->write((const void *)msg, msgsize);

  free(msg);
  return w;
}
