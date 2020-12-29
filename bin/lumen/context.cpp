#include <errno.h>
#include <gfx/disjoint_rects.h>
#include <gfx/font.h>
#include <gfx/image.h>
#include <gfx/scribe.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <math.h>
#include <string.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "internal.h"

// run the compositor at 60fps if there is work to be done.
#define COMPOSE_INTERVAL (1000 / 60)

#define USE_COMPOSE_INTERVAL

#define min(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })


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

  wallpaper = gfx::load_png("/usr/res/lumen/wallpaper.png");

  if (wallpaper->width() != screen.width() || wallpaper->height() != screen.height()) {
    wallpaper = wallpaper->scale(screen.width(), screen.height(), gfx::bitmap::SampleMode::Nearest);
  }


  server.listen("/usr/servers/lumen", [this] { accept_connection(); });

#ifdef USE_COMPOSE_INTERVAL
  compose_timer = ck::timer::make_interval(COMPOSE_INTERVAL, [this] { this->compose(); });
#else
  pthread_create(&compositor_thread, NULL, lumen::context::compositor_thread_worker, (void *)this);
#endif
  invalidate(screen.bounds());
}


void lumen::context::handle_keyboard_input(keyboard_packet_t &pkt) {
  if (focused_window != NULL) {
    focused_window->handle_keyboard_input(pkt);
  }
}

void lumen::context::handle_mouse_input(struct mouse_packet &pkt) {
  windows_lock.lock();
  auto old_pos = screen.mouse_pos;
  invalidate(screen.mouse_rect());
  screen.handle_mouse_input(pkt);
  invalidate(screen.mouse_rect());

  // printf("mouse took %lldus\n", sysbind_gettime_microsecond() - pkt.timestamp);

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
    hovered_window->window_lock.lock();
    if (!dragging) {
      auto pos =
          gfx::point(screen.mouse_pos.x() - hovered_window->rect.x, screen.mouse_pos.y() - hovered_window->rect.y);


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
    hovered_window->window_lock.unlock();
  } else {
    screen.cursor = mouse_cursor::pointer;
  }


  windows_lock.unlock();
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
  dirty_regions_lock.lock();
  dirty_regions.add(real);
  dirty_regions_lock.unlock();

#ifdef USE_COMPOSE_INTERVAL
  if (!compose_timer->running()) {
    compose_timer->start(COMPOSE_INTERVAL, true);
  }
#endif
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


#define HANDLE_TYPE(t, data_type) if (auto arg = (data_type *)msg.data; msg.type == t && msg.len == sizeof(data_type))




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

    win->pending_invalidation_id = msg.id;

    for (int i = 0; i < min(arg->nrects, MAX_INVALIDATE); i++) {
      auto &r = arg->rects[i];
      // offset to the location that the window is at
      r.x += win->rect.x;
      r.y += win->rect.y;

      auto rect = gfx::rect(r.x, r.y, r.w, r.h).intersect(win->rect);
      win->translate_invalidation(rect);
      if (!rect.is_empty()) {
        r.w++;
        // r.h++;
        invalidate(rect);
      }
    }

    return;
  };


  HANDLE_TYPE(LUMEN_MSG_RESIZE, lumen::resize_msg) {
    struct lumen::resized_msg res;
    res.id = -1;

    auto *win = c.windows[arg->id];
    if (win == NULL) {
      c.respond(msg, msg.type, res);
      return;
    }

    auto new_bitmap = ck::make_ref<gfx::shared_bitmap>(arg->width, arg->height);
    // update the window
    invalidate(win->rect);
    win->bitmap = new_bitmap;
    win->set_mode(win->mode);
    invalidate(win->rect);

    res.id = win->id;
    res.width = arg->width;
    res.height = arg->height;
    strncpy(res.bitmap_name, win->bitmap->shared_name(), LUMEN_NAMESZ - 1);

    c.respond(msg, msg.type, res);
    return;
  }


  printf("message %d of type %d not handled. Ignored.\n", msg.id, msg.type);
}




void lumen::context::window_opened(lumen::window *w) {
  // TODO lock

  windows_lock.lock();
  // [sanity check] make sure the window isn't already in the list :^)
  for (auto *e : windows) {
    if (w == e) {
      fprintf(stderr, "Window already exists!!\n");
      exit(EXIT_FAILURE);
    }
  }



  long wleft = (screen.width() - w->rect.w);
  long hleft = (screen.height() - w->rect.h);

  if (wleft > 0) {
    w->rect.x = rand() % wleft;
  } else {
    w->rect.x = 100;
  }

  if (hleft > 0) {
    w->rect.y = rand() % hleft;
  } else {
    w->rect.y = 100;
  }


  // insert at the back (front of the stack)
  windows.push(w);

  // TODO: set the focused window to the new one
  select_window(w);
  windows_lock.unlock();
}


void lumen::context::window_closed(lumen::window *w) {
  windows_lock.lock();
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
  windows_lock.unlock();
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




void *lumen::context::compositor_thread_worker(void *arg) {
  lumen::context *self = (lumen::context *)arg;
  while (1) {
    self->compose();
    usleep(1000);
  }
}



ck::vec<gfx::rect> difference(gfx::rect rectA, gfx::rect rectB) {
  if (rectB.is_empty() || !rectA.intersects(rectB) || rectB.contains(rectA)) return {};

  ck::vec<gfx::rect> result;
  gfx::rect top = {}, bottom = {}, left = {}, right = {};
  int rectCount = 0;

  // compute the top rectangle
  int raHeight = rectB.y - rectA.y;
  if (raHeight > 0) {
    top = gfx::rect(rectA.x, rectA.y, rectA.w, raHeight);
    rectCount++;
  }

  // compute the bottom rectangle
  int rbY = rectB.y + rectB.h;
  int rbHeight = rectA.h - (rbY - rectA.y);
  if (rbHeight > 0 && rbY < rectA.y + rectA.h) {
    bottom = gfx::rect(rectA.x, rbY, rectA.w, rbHeight);
    rectCount++;
  }

  int rectAYH = rectA.y + rectA.h;
  int y1 = rectB.y > rectA.y ? rectB.y : rectA.y;
  int y2 = rbY < rectAYH ? rbY : rectAYH;
  int rcHeight = y2 - y1;

  // compute the left rectangle
  int rcWidth = rectB.x - rectA.x;
  if (rcWidth > 0 && rcHeight > 0) {
    left = gfx::rect(rectA.x, y1, rcWidth, rcHeight);
    rectCount++;
  }

  // compute the right rectangle
  int rbX = rectB.x + rectB.w;
  int rdWidth = rectA.w - (rbX - rectA.x);
  if (rdWidth > 0) {
    right = gfx::rect(rbX, y1, rdWidth, rcHeight);
    rectCount++;
  }

  if (!top.is_empty()) result.push(top);
  if (!bottom.is_empty()) result.push(bottom);
  if (!left.is_empty()) result.push(left);
  if (!right.is_empty()) result.push(right);
  return result;
}



static long frame = 0;
void lumen::context::compose(void) {
  frame++;

  /*
// do nothing if nothing has changed
  */

#ifdef USE_COMPOSE_INTERVAL
  // compose_timer->stop();
#endif
  // printf("compose\n");

  if (dirty_regions.size() == 0) {
    // printf("nothing to do...\n");
    return;
  }

  // make a tmp bitmap
  gfx::bitmap b(screen.width(), screen.height(), screen.buffer());

  // this scribe is the "compositor scribe", used by everyone in this function
  gfx::scribe scribe(b);




  bool draw_mouse = screen.mouse_moved;
  // clear that information
  if (draw_mouse) screen.mouse_moved = false;

  windows_lock.lock();
  dirty_regions_lock.lock();
  for (auto &r : dirty_regions.rects()) {
    if (r.intersects(screen.mouse_rect())) draw_mouse = true;
    scribe.blit(gfx::point(r.x, r.y), *wallpaper, r);
  }

  // go back to front and compose each window
  for (auto win : windows) {
    win->window_lock.lock();
    // make a state for this window
    scribe.enter();
    scribe.state().offset = gfx::point(win->rect.x, win->rect.y);

    for (auto &dirty_rect : dirty_regions.rects()) {
      if (!dirty_rect.intersects(win->rect)) continue;
      // is this region occluded by another window?
      // if (occluded(window, dirty_rect)) {
      // continue;
      // }
      scribe.state().clip = dirty_rect;

      win->draw(scribe);
    }
    scribe.leave();
    win->window_lock.unlock();
  }

  if (draw_mouse) {
    screen.draw_mouse();
  }



  int sw = screen.width();
  // copy the changes we made to the other buunffer
  for (auto &r : dirty_regions.rects()) {
    auto off = r.y * sw + r.x;
    uint32_t *to_ptr = screen.front_buffer + off;
    uint32_t *from_ptr = screen.back_buffer + off;

    for (int y = 0; y < r.h; y++) {
      // explicit looping optimizes more
      for (int i = 0; i < r.w; i++) to_ptr[i] = from_ptr[i];
      from_ptr += sw;
      to_ptr += sw;
    }
  }

  // and now, go through all windows and notify them they have been composed :)
  for (auto win : windows) {
    win->window_lock.lock();
    if (win->pending_invalidation_id != -1) {
      struct lumen::invalidated_msg m;
      m.id = win->id;
      win->guest.guest_lock.lock();
      win->guest.send_raw(LUMEN_MSG_WINDOW_INVALIDATED, win->pending_invalidation_id, &m, sizeof(m));
      win->guest.guest_lock.unlock();
      win->pending_invalidation_id = -1;
    }
    win->window_lock.unlock();
  }

  dirty_regions.clear();

  dirty_regions_lock.unlock();
  windows_lock.unlock();
}




lumen::guest::guest(long id, struct context &ctx, ck::ipcsocket *conn) : id(id), ctx(ctx), connection(conn) {
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


  if (w >= ctx.screen.width()) {
    win->rect.x = 0;
  } else {
    win->rect.x = rand() % (ctx.screen.width() - w);
  }


  if (h >= ctx.screen.height()) {
    win->rect.y = 0;
  } else {
    win->rect.y = rand() % (ctx.screen.height() - h);
  }
  windows.set(win->id, win);

  ctx.window_opened(win);
  return win;
}

void lumen::guest::process_message(lumen::msg &msg) {
  guest_lock.lock();
  // defer to the window server's function
  ctx.process_message(*this, msg);
  guest_lock.unlock();
}


long lumen::guest::send_raw(int type, int id, void *payload, size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = id;
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy((void *)(msg + 1), payload, payloadsize);

  auto w = connection->send((void *)msg, msgsize, MSG_DONTWAIT);

  free(msg);
  return w;
}
