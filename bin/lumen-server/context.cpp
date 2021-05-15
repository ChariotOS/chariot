#include <ck/cmp.h>
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
#define FPS (60)
#define COMPOSE_INTERVAL (1000 / FPS)

#define USE_COMPOSE_INTERVAL



extern char **environ;
pid_t spawn(const char *command) {
  int pid = fork();
  if (pid == 0) {
    const char *args[] = {"/bin/sh", "-c", (char *)command, NULL};

    // debug_hexdump(args, sizeof(args));
    execve("/bin/sh", args, (const char **)environ);
    exit(-1);
  }
  return pid;
}


lumen::context::context(void) : screen(1024, 768) {
  if (keyboard.open("/dev/keyboard", "r+")) {
    keyboard.on_read([this] {
      while (1) {
        keyboard_packet_t pkt;
        keyboard.read(&pkt, sizeof(pkt));
        if (errno == EAGAIN) break;
        handle_keyboard_input(pkt);
      }
    });
  }

  int x = 0;
  unsigned long y = 1;
  auto out = ck::max(x, y);

  if (mouse.open("/dev/mouse", "r+")) {
    mouse.on_read([this] {
      while (1) {
        struct mouse_packet pkt;
        mouse.read(&pkt, sizeof(pkt));
        if (errno == EAGAIN) break;
        handle_mouse_input(pkt);
      }

      // get lower latencies
      // this->compose();
    });
  }

  bool load_wallpaper = true;
  if (load_wallpaper) {
    wallpaper = gfx::load_image("/usr/res/lumen/wallpaper.jpg");
    // wallpaper = gfx::load_png("/usr/res/lumen/wallpaper.png");
    if (wallpaper->width() != screen.width() || wallpaper->height() != screen.height()) {
      wallpaper =
          wallpaper->scale(screen.width(), screen.height(), gfx::bitmap::SampleMode::Nearest);
    }
  } else {
    wallpaper = new gfx::bitmap(screen.width(), screen.height());
    int w = screen.width();
    int h = screen.height();
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        uint32_t c = rand();

        int n = ((x - (w / 2)) * (w / 4));
        int m = (y - (h / 2));
        if (m != 0 && n % m) c = 0;
        wallpaper->set_pixel(x, y, c);
      }
    }
    /*
for (int i = 0; i < screen.width() * screen.height(); i++) {
wallpaper->pixels()[i] = 0x333333;
}
    */
  }

  server.listen("/usr/servers/lumen", [this] { accept_connection(); });

  compose_timer = ck::timer::make_interval(COMPOSE_INTERVAL, [this] { this->compose(); });
  invalidate(screen.bounds());

	// spawn("term");
  // spawn("fluidsim");
  // spawn("doom");
}


void lumen::context::handle_keyboard_input(keyboard_packet_t &pkt) {
  if (focused_window != NULL) {
    focused_window->handle_keyboard_input(pkt);
  }
}

void lumen::context::handle_mouse_input(struct mouse_packet &pkt) {
  auto old_pos = screen.mouse_pos;
  invalidate(screen.mouse_rect());
  screen.handle_mouse_input(pkt);
  invalidate(screen.mouse_rect());

  int dx = screen.mouse_pos.x() - old_pos.x();
  int dy = screen.mouse_pos.y() - old_pos.y();


  /* What buttons are pressed? */
  int clicked = (pkt.buttons & (MOUSE_LEFT_CLICK | MOUSE_RIGHT_CLICK));
  /* What buttons where pressed */
  int pressed = ~mouse_down & clicked;
  /* And which were released */
  int released = mouse_down & ~clicked;
  int prev_down = mouse_down;
  /* Update the current click state */
  mouse_down = clicked;

  /* If you aren't currently clicking, calculate a new hovered window.
   * This makes sure the last window gets all mouse events while the mouse is held on it
   */
  if (prev_down == 0) calculate_hover();


  assert((released & pressed) == 0);


  if (dragging && !(mouse_down & MOUSE_LEFT_CLICK)) {
    dragging = false;
    // TODO: pull from window's state
  }

  if (focused_window != hovered_window) {
    if (pressed & (MOUSE_LEFT_CLICK | MOUSE_RIGHT_CLICK)) {
      select_window(hovered_window);
      /* TODO: notify the window of it's new focus state :) */
    }
  }

  if (hovered_window != NULL) {
    auto mrel = gfx::point(screen.mouse_pos.x() - hovered_window->rect.x,
                           screen.mouse_pos.y() - hovered_window->rect.y);


    hovered_window->last_hover = mrel;

    if (hovered_window == focused_window) {
      /* Deal with pointer events */
      hovered_window->mouse_down = mouse_down;

      if (pressed & MOUSE_LEFT_CLICK) hovered_window->last_lclick = mrel;
      if (pressed & MOUSE_RIGHT_CLICK) hovered_window->last_rclick = mrel;


      struct lumen::input_msg m;
      m.window_id = hovered_window->id;
      m.type = LUMEN_INPUT_MOUSE;

      m.mouse.dx = dx;
      m.mouse.dy = dy;
      m.mouse.hx = mrel.x();
      m.mouse.hy = mrel.y();

      m.mouse.buttons = pkt.buttons;
      hovered_window->guest.send_msg(LUMEN_MSG_INPUT, m);
    }


#if 0
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
			printf("dragging from (%3d %3d) by (%d, %d)\n");
			/*
      screen.cursor = mouse_cursor::grabbing;
      int dx = screen.mouse_pos.x() - old_pos.x();
      int dy = screen.mouse_pos.y() - old_pos.y();
      move_window(hovered_window, dx, dy);
			*/
    }
#endif
  } else {
    screen.cursor = mouse_cursor::pointer;
  }
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
  dirty_regions.add(real);

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


#define HANDLE_TYPE(t, data_type) \
  if (auto arg = (data_type *)msg.data; msg.type == t && msg.len == sizeof(data_type))



void lumen::context::move_window(lumen::window *win, int dx, int dy) {
  /* Invalidate the old position */
  invalidate(win->rect);

  win->rect.x += dx;
  win->rect.y += dy;

  /* Make sure the window doesn't go off the top of the screen */
  if (win->rect.y < 0) win->rect.y = 0;

  /* Right check */
  if (win->rect.x > screen.width() - 1) win->rect.x = screen.width() - 1 - win->rect.w;

  /* Left check */
  if (win->rect.right() <= 0) win->rect.x = 1 - win->rect.w;

  /* Bottom Check */
  if (win->rect.y > screen.height()) win->rect.y = screen.height() - 1;

  /* And the new position :) */
  invalidate(win->rect);
}


void lumen::context::process_message(lumen::guest &c, lumen::msg &msg) {
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

    if (arg->sync) {
      win->pending_invalidation_id = msg.id;
    } else {
      win->pending_invalidation_id = -1;
    }

    for (int i = 0; i < ck::min(arg->nrects, MAX_INVALIDATE); i++) {
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

  HANDLE_TYPE(LUMEN_MSG_MOVEREQ, lumen::move_request) {
    auto *win = c.windows[arg->id];
    if (win == NULL) return;

    move_window(win, arg->dx, arg->dy);

    return;
  }

  HANDLE_TYPE(LUMEN_MSG_RESIZE, lumen::resize_msg) {
    struct lumen::resized_msg res;
    res.id = -1;

    auto *win = c.windows[arg->id];
    if (win == NULL) {
      c.respond(msg, msg.type, res);
      return;
    }

    auto new_bitmap = ck::make_ref<gfx::shared_bitmap>(arg->width, arg->height);

    {
      /*
       * poorly copy the old bitmap to the new one, to reduce flicker
       * (and to force the pages to be allocated before rendering)
       */

      auto start = sysbind_gettime_microsecond();
      int sw = ck::min(arg->width, win->rect.w);
      int sh = ck::min(arg->height, win->rect.h);
      auto *pix = new_bitmap->pixels();
      for (int i = 0; i < arg->width * arg->height; i++)
        pix[i] = 0xFF00FF;
      for (int y = 0; y < sh; y++) {
        const uint32_t *src = win->bitmap->scanline(y);
        uint32_t *dst = new_bitmap->scanline(y);
        memcpy(dst, src, sw * sizeof(uint32_t));
      }
    }

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
}


void lumen::context::window_closed(lumen::window *w) {
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
  compose_timer->stop();
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


#if 0
	printf("compose %d rects\n", dirty_regions.size());
	for (auto &r : dirty_regions.rects()) {
		printf(" - %3d, %3d, %3d, %3d\n", r.x, r.y, r.w, r.h);
	}
#endif

  bool draw_mouse = screen.mouse_moved;
  // clear that information
  if (draw_mouse) screen.mouse_moved = false;

  auto start = sysbind_gettime_microsecond();

  {
    uint32_t *wp = wallpaper->pixels();
    for (auto &r : dirty_regions.rects()) {
      if (r.intersects(screen.mouse_rect())) draw_mouse = true;
      // scribe.blit(gfx::point(r.x, r.y), *wallpaper, r);


      int sw = screen.width();
      auto off = r.y * sw + r.x;
      uint32_t *to_ptr = b.pixels() + off;
      uint32_t *from_ptr = wp + off;

      for (int y = 0; y < r.h; y++) {
        // explicit looping optimizes more
        for (int i = 0; i < r.w; i++)
          to_ptr[i] = from_ptr[i];
        from_ptr += sw;
        to_ptr += sw;
      }
    }
  }

  // go back to front and compose each window
  for (auto win : windows) {
    // make a state for this window
    scribe.enter();
    scribe.state().offset = gfx::point(win->rect.x, win->rect.y);

    for (auto &dirty_rect : dirty_regions.rects()) {
      if (dirty_rect.intersects(screen.mouse_rect())) draw_mouse = true;
      if (!dirty_rect.intersects(win->rect)) continue;
      scribe.state().clip = dirty_rect;

      win->draw(scribe);
    }
    scribe.leave();
  }

  if (draw_mouse) {
    screen.draw_mouse();
  }

  if (!screen.hardware_double_buffered()) {
    int sw = screen.width();
    // copy the changes we made to the other buffer
    for (auto &r : dirty_regions.rects()) {
      auto off = r.y * sw + r.x;
      uint32_t *to_ptr = screen.front_buffer + off;
      uint32_t *from_ptr = screen.back_buffer + off;


      for (int y = 0; y < r.h; y++) {
        // explicit looping optimizes more
        for (int i = 0; i < r.w; i++)
          to_ptr[i] = from_ptr[i];
        from_ptr += sw;
        to_ptr += sw;
      }
    }
  } else {
    screen.flush_fb();
  }

  dirty_regions.clear();

  // and now, go through all windows and notify them they have been composed :)
  for (auto win : windows) {
    if (win->pending_invalidation_id != -1) {
      struct lumen::invalidated_msg m;
      m.id = win->id;
      win->guest.send_raw(LUMEN_MSG_WINDOW_INVALIDATED, win->pending_invalidation_id, &m,
                          sizeof(m));
      win->pending_invalidation_id = -1;
    }
  }
}




lumen::guest::guest(long id, struct context &ctx, ck::ipcsocket *conn)
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
  // defer to the window server's function
  ctx.process_message(*this, msg);
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
