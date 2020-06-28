#include <errno.h>
#include <gfx/image.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "internal.h"

lumen::context::context(void) : screen(640, 480) {
  // clear the screen
  // memset(screen.pixels(), 0, screen.screensize());

  // screen.clear(0x333333);




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


  compose_timer =
      ck::timer::make_interval(1000 / 30.0, [this] { this->compose(); });

  invalidate(screen.bounds());
}


void lumen::context::handle_keyboard_input(keyboard_packet_t &pkt) {
  printf("keyboard: code: %02x ch: %02x (%c)\n", pkt.key, pkt.character,
         pkt.character);
}

void lumen::context::handle_mouse_input(struct mouse_packet &pkt) {
  invalidate(screen.mouse_rect());
  screen.handle_mouse_input(pkt);
}


void lumen::context::invalidate(const gfx::rect &r) { dirty_regions.push(r); }


void lumen::context::accept_connection() {
  auto id = next_guest_id++;
  // accept the connection
  auto *guest = server.accept();
  guests.set(id, new lumen::guest(id, *this, guest));
}

void lumen::context::guest_closed(long id) {
  auto c = guests[id];
  guests.remove(id);
  delete c;
}


#define HANDLE_TYPE(t, data_type)       \
  if (auto arg = (data_type *)msg.data; \
      msg.type == t && msg.len == sizeof(data_type))


void lumen::context::process_message(lumen::guest &c, lumen::msg &msg) {
  // compose();  // XXX: remove me!
  HANDLE_TYPE(LUMEN_MSG_CREATE_WINDOW, lumen::create_window_msg) {
    (void)arg;

    ck::string name(arg->name, LUMEN_NAMESZ);

    printf("window wants to be made! ('%s', %dx%d)\n", name.get(), arg->width,
           arg->height);

    struct lumen::window_created_msg res;
    // TODO: figure out a better position to open to
    auto *window = c.new_window(name, arg->width, arg->height);
    if (window != NULL) {
      res.window_id = window->id;
      strncpy(res.bitmap_name, window->bitmap->shared_name(), LUMEN_NAMESZ - 1);
    } else {
      res.window_id = -1;
    }
    c.respond(msg, LUMEN_MSG_WINDOW_CREATED, res);
    return;
  }

  HANDLE_TYPE(LUMEN_MSG_GREET, lumen::greet_msg) {
    (void)arg;
    // responed to that
    struct lumen::greetback_msg res;
    res.magic = LUMEN_GREETBACK_MAGIC;
    res.guest_id = c.id;
    c.respond(msg, msg.type, res);
  };
}




void lumen::context::window_opened(lumen::window *w) {
  // TODO lock

  // [sanity check] make sure the window isn't already in the list :^)
  for (auto *e : windows) {
    if (w == e) {
      fprintf(stderr, "Window already exists!!\n");
      exit(EXIT_FAILURE);
    }
  }

  windows.insert(0, w);
  // TODO: set the focused window to the new one
}


void lumen::context::window_closed(lumen::window *w) {
  // TODO lock

  // Remove the window from our list
  for (int i = 0; i < windows.size(); i++) {
    if (windows[i] == w) {
      windows.remove(i);
      break;
    }
  }
}

static long frame = 0;
void lumen::context::compose(void) {
  // printf("compose frame %d\n", frame);
  frame++;

  // make a tmp bitmap
  gfx::bitmap b(screen.width(), screen.height(), screen.buffer());

	/*
  b.clear(0x333333);

  gfx::rect r;
  r.x = frame;
  r.y = 0; // os(frame / 20.0) * 300;
  r.w = 200;
  r.h = 200;
  r.x += 200;
  r.y += 200;
  b.fill_rect(r, 30, 0xFFFFFF);

  screen.flip_buffers();

	frame %= screen.width();

  return;

	*/

  // do nothing if nothing has changed
  if (dirty_regions.size() == 0) return;


  // b.clear(0x333333);

  for (auto &r : dirty_regions) {
    b.fill_rect(r, 0x333333);
  }


  // b.draw_rect(gfx::rect(0, 0, screen.mouse_pos.x(), screen.mouse_pos.y()),
  // 30, 0xFFFFFF);
  screen.draw_mouse();

  screen.flip_buffers();

  int sw = screen.width();

  // copy the changes we made to the other buffer
  for (auto &r : dirty_regions) {
    int ox = r.left();
    int oy = r.top();

    // printf("copy: %4d %4d %4d %4d\n", r.x, r.y, r.w, r.h);

    for (int y = 0; y <= r.h; y++) {
      int ry = y + oy;
      if (ry < 0) continue;
      if (ry > screen.height()) break;
      for (int x = 0; x <= r.w; x++) {
        int rx = x + ox;
        if (rx < 0 || rx > sw) continue;
        screen.back_buffer[rx + ry * sw] = screen.front_buffer[rx + ry * sw];
      }
    }
  }

  dirty_regions.clear();

	asm("pause");
}




lumen::guest::guest(long id, struct context &ctx, ck::localsocket *conn)
    : id(id), ctx(ctx), connection(conn) {
  // printf("got a connection\n");

  connection->on_read([this] { this->on_read(); });
}

lumen::guest::~guest(void) {
  for (auto kv : windows) {
    printf("window '%s' (id: %d) removed due to guest being destructed!\n",
           kv.value->name.get(), kv.key);

    ctx.window_closed(kv.value);
    delete kv.value;
  }
  delete connection;
}

void lumen::guest::on_read(void) {
  bool failed = false;
  auto msgs = lumen::drain_messages(*connection, failed);
  // handle messages

  for (auto *msg : msgs) {
    process_message(*msg);
    free(msg);
  }

  // if the guest is at EOF *or* it otherwise failed, consider it
  // disconnected
  if (connection->eof() || failed) {
    this->ctx.guest_closed(id);
  }
}



struct lumen::window *lumen::guest::new_window(ck::string name, int w, int h) {
  auto id = next_window_id++;
  auto win = new lumen::window(id, *this, w, h);
  win->name = name;

  // XXX: allow the user to request a position
  win->rect.x = rand() % (ctx.screen.width() - w);
  win->rect.y = rand() % (ctx.screen.height() - h);

  printf("guest %d made a new window, '%s'\n", this->id, name.get());
  windows.set(win->id, win);


  ctx.window_opened(win);
  return win;
}

void lumen::guest::process_message(lumen::msg &msg) {
  // defer to the window server's function
  ctx.process_message(*this, msg);
}


long lumen::guest::send_raw(int type, int id, void *payload,
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
