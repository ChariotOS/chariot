#include <errno.h>
#include <gfx/font.h>
#include <gfx/image.h>
#include <gfx/scribe.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <math.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "internal.h"

// run the compositor at 60fps if there is work to be done.
#define COMPOSE_INTERVAL (1000 / 1000)


lumen::context::context(void) : screen(1024, 768) {
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
    }
  });


  server.listen("/usr/servers/lumen", [this] { accept_connection(); });


  compose_timer =
      ck::timer::make_interval(COMPOSE_INTERVAL, [this] { this->compose(); });

  invalidate(screen.bounds());
}


void lumen::context::handle_keyboard_input(keyboard_packet_t &pkt) {
  printf("keyboard: code: %02x ch: %02x (%c)\n", pkt.key, pkt.character,
         pkt.character);
}

void lumen::context::handle_mouse_input(struct mouse_packet &pkt) {
  auto old_pos = screen.mouse_pos;
  invalidate(screen.mouse_rect());
  screen.handle_mouse_input(pkt);
  invalidate(screen.mouse_rect());

  if (!dragging) calculate_hover();

  if (dragging && !(pkt.buttons & MOUSE_LEFT_CLICK)) {
    dragging = false;
    // TODO: pull from window's state
  }

  if (focused_window != hovered_window) {
    if (pkt.buttons & (MOUSE_LEFT_CLICK | MOUSE_RIGHT_CLICK)) {
      select_window(hovered_window);
    }
  }

  if (hovered_window) {
    if (!dragging) {
      auto pos = gfx::point(screen.mouse_pos.x() - hovered_window->rect.x,
                            screen.mouse_pos.y() - hovered_window->rect.y);


      int res = hovered_window->handle_mouse_input(pos, pkt);
      if (res == WINDOW_REGION_DRAG) {
        screen.cursor = mouse_cursor::grab;
        if (pkt.buttons & MOUSE_LEFT_CLICK) {
          dragging = true;
        }
      } else if (res == WINDOW_REGION_NORM) {
				screen.cursor = mouse_cursor::pointer;
			} else if (res == WINDOW_REGION_CLOSE) {
				screen.cursor = mouse_cursor::pointer;
			}
    }

    if (dragging) {
      screen.cursor = mouse_cursor::grabbing;
      invalidate(hovered_window->rect);
      hovered_window->rect.x -= old_pos.x() - screen.mouse_pos.x();
      hovered_window->rect.y -= old_pos.y() - screen.mouse_pos.y();
      invalidate(hovered_window->rect);
		}
  } else {
    screen.cursor = mouse_cursor::pointer;
  }


	// compose asap so we can get lower mouse input latencies
	compose();
}


void lumen::context::calculate_hover(void) {
  auto *old = hovered_window;

  auto mp = screen.mouse_pos;


  bool found = false;
  for (int i = windows.size() - 1; i >= 0; i--) {
    auto win = windows[i];

    if (win->rect.contains(mp.x(), mp.y())) {
      hovered_window = win;
      found = true;
      break;
    }
  }

  if (found == false) {
    hovered_window = NULL;
  }

  if (old != hovered_window) {
    // notify both!
    if (old) {
      old->hovered = false;
      invalidate(old->rect);
    }
    if (hovered_window) {
      hovered_window->hovered = true;
      invalidate(hovered_window->rect);
    }
  }
}


void lumen::context::select_window(lumen::window *win) {
  // de-select all windows
  if (win == NULL) {
    // TODO: tell the window it lost focus
  } else {
    if (focused_window != win) {
      int from_ind = 0;
      for (from_ind = 0; from_ind < windows.size(); from_ind++) {
        if (windows[from_ind] == win) {
          break;
        }
      }
      // the focused window is no longer focused
      if (focused_window != NULL) {
        focused_window->focused = false;
        invalidate(focused_window->rect);
      }
      windows.remove(from_ind);
      windows.push(win);
      win->focused = true;
    }
  }

  if (win != NULL) invalidate(win->rect);
  if (focused_window != NULL) invalidate(focused_window->rect);
  focused_window = win;
}

void lumen::context::invalidate(const gfx::rect &r) {
  auto real = r.intersect(screen.bounds());
  for (auto &d : dirty_regions) {
    if (real.contains(d)) {
      d = real;
      return;
    }
    // the region is already invalidated
    if (d.contains(real)) {
      return;
    }
  }
  dirty_regions.push(real);

  if (!compose_timer->running()) {
    compose_timer->start(COMPOSE_INTERVAL, true);
  }
}


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


    struct lumen::window_created_msg res;
    // TODO: figure out a better position to open to
    auto *window = c.new_window(name, arg->width, arg->height);

    if (window != NULL) {
      res.window_id = window->id;
      strncpy(res.bitmap_name, window->bitmap->shared_name(), LUMEN_NAMESZ - 1);
    } else {
      res.window_id = -1;
    }

    invalidate(window->rect);

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
    return;
  };


  HANDLE_TYPE(LUMEN_MSG_WINDOW_INVALIDATE, lumen::invalidate_msg) {
    auto *win = c.windows[arg->id];
    if (win == NULL) return;

    // offset to the location that the window is at
    arg->x += win->rect.x;
    arg->y += win->rect.y;

    auto r = gfx::rect(arg->x, arg->y, arg->w, arg->h).intersect(win->rect);
    win->translate_invalidation(r);
    invalidate(r);
    return;
  };


  printf("message %d of type %d not handled. Ignored.\n", msg.id, msg.type);
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



  w->rect.x = rand() % (screen.width() - w->rect.w);
  w->rect.y = rand() % (screen.height() - w->rect.h);


  // insert at the back (front of the stack)
  windows.push(w);

  // TODO: set the focused window to the new one
  select_window(w);
}


void lumen::context::window_closed(lumen::window *w) {
  // TODO lock

  // Remove the window from our list
  for (int i = 0; i < windows.size(); i++) {
    if (windows[i] == w) {
      windows.remove(i);
      invalidate(w->rect);
      break;
    }
  }


  if (hovered_window == w) {
    hovered_window = NULL;
    calculate_hover();
  }


  if (focused_window == w) {
    focused_window = NULL;
    calculate_hover();
  }
}


bool lumen::context::occluded(lumen::window &win, const gfx::rect &a) {
  auto r = win.rect.intersect(a);


  for (int i = windows.size() - 1; i >= 0; i--) {
    auto other = windows[i];
    if (&win == other) {
      return false;
    }
    if (other->rect.contains(r)) {
      return true;
    }
  }
  return false;
}


// #define LUMEN_DEBUG

#ifdef LUMEN_DEBUG
ck::ref<gfx::font> get_debug_font(void) {
  static ck::ref<gfx::font> font;
  if (!font) font = gfx::font::open("scientifica-normal", 11);
  return font;
}
#endif




static long frame = 0;
void lumen::context::compose(void) {
  frame++;

  // make a tmp bitmap
  gfx::bitmap b(screen.width(), screen.height(), screen.buffer());

  // do nothing if nothing has changed
  if (dirty_regions.size() == 0) return;

  // this scribe is the "compositor scribe", used by everyone in this function
  gfx::scribe scribe(b);


  bool draw_mouse = screen.mouse_moved;
  // clear that information
  if (draw_mouse) screen.mouse_moved = false;

  for (auto &r : dirty_regions) {
    if (r.intersects(screen.mouse_rect())) draw_mouse = true;
    scribe.fill_rect(r, 0x6261a1);
  }


  auto compose_window = [&](lumen::window &window) -> bool {
    // make a state for this window
    scribe.enter();
    scribe.state().offset = gfx::point(window.rect.x, window.rect.y);

    for (auto &dirty_rect : dirty_regions) {
      if (!dirty_rect.intersects(window.rect)) continue;
      // is this region occluded by another window?
      if (occluded(window, dirty_rect)) {
        continue;
      }
      scribe.state().clip = dirty_rect;

      window.draw(scribe);
    }
    scribe.leave();
    return true;
  };

  // go back to front
  for (auto win : windows) {
    compose_window(*win);
  }

  if (draw_mouse) {
    // scribe.draw_frame(screen.mouse_rect(), 0xFF8888);
    screen.draw_mouse();
  }


  int sw = screen.width();
  // copy the changes we made to the other buffer
  for (auto &r : dirty_regions) {
    auto off = r.y * sw + r.x;
    uint32_t *to_ptr = screen.front_buffer + off;
    uint32_t *from_ptr = screen.back_buffer + off;

    for (int y = 0; y < r.h; y++) {
      memcpy(to_ptr, from_ptr, r.w * sizeof(uint32_t));
      from_ptr += sw;
      to_ptr += sw;
    }
  }
  dirty_regions.clear();

  compose_timer->stop();

	// printf("took %zu\n", tsc() - start);
}




lumen::guest::guest(long id, struct context &ctx, ck::localsocket *conn)
    : id(id), ctx(ctx), connection(conn) {
  connection->on_read([this] { this->on_read(); });
}

lumen::guest::~guest(void) {
  for (auto kv : windows) {
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
