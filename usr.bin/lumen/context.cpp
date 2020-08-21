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
  if (focused_window != NULL) {
    focused_window->handle_keyboard_input(pkt);
  }
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
    if (!r.is_empty()) {
      r.w++;
      // r.h++;
      invalidate(r);
    }
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




static unsigned short const stackblur_mul[255] = {
    512, 512, 456, 512, 328, 456, 335, 512, 405, 328, 271, 456, 388, 335, 292,
    512, 454, 405, 364, 328, 298, 271, 496, 456, 420, 388, 360, 335, 312, 292,
    273, 512, 482, 454, 428, 405, 383, 364, 345, 328, 312, 298, 284, 271, 259,
    496, 475, 456, 437, 420, 404, 388, 374, 360, 347, 335, 323, 312, 302, 292,
    282, 273, 265, 512, 497, 482, 468, 454, 441, 428, 417, 405, 394, 383, 373,
    364, 354, 345, 337, 328, 320, 312, 305, 298, 291, 284, 278, 271, 265, 259,
    507, 496, 485, 475, 465, 456, 446, 437, 428, 420, 412, 404, 396, 388, 381,
    374, 367, 360, 354, 347, 341, 335, 329, 323, 318, 312, 307, 302, 297, 292,
    287, 282, 278, 273, 269, 265, 261, 512, 505, 497, 489, 482, 475, 468, 461,
    454, 447, 441, 435, 428, 422, 417, 411, 405, 399, 394, 389, 383, 378, 373,
    368, 364, 359, 354, 350, 345, 341, 337, 332, 328, 324, 320, 316, 312, 309,
    305, 301, 298, 294, 291, 287, 284, 281, 278, 274, 271, 268, 265, 262, 259,
    257, 507, 501, 496, 491, 485, 480, 475, 470, 465, 460, 456, 451, 446, 442,
    437, 433, 428, 424, 420, 416, 412, 408, 404, 400, 396, 392, 388, 385, 381,
    377, 374, 370, 367, 363, 360, 357, 354, 350, 347, 344, 341, 338, 335, 332,
    329, 326, 323, 320, 318, 315, 312, 310, 307, 304, 302, 299, 297, 294, 292,
    289, 287, 285, 282, 280, 278, 275, 273, 271, 269, 267, 265, 263, 261, 259};

static unsigned char const stackblur_shr[255] = {
    9,  11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17,
    17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22,
    22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
    22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24};



#define clamp(__v, __lower, __upper) (MAX(MIN((__v), (__upper)), (__lower)))

#define MIN(__x, __y) ((__x) < (__y) ? (__x) : (__y))

#define MAX(__x, __y) ((__x) > (__y) ? (__x) : (__y))


void stackblurJob(
    unsigned char *src,   ///< input image data
    unsigned int w,       ///< image width
    unsigned int h,       ///< image height
    unsigned int radius,  ///< blur intensity (should be in 2..254 range)
    unsigned int minX, unsigned int maxX, unsigned int minY,
    unsigned int maxY) {
  (void)h;

  unsigned int x, y, xp, yp, i;
  unsigned int sp;
  unsigned int stack_start;
  unsigned char *stack_ptr;

  unsigned char *src_ptr;
  unsigned char *dst_ptr;

  unsigned long sum_r;
  unsigned long sum_g;
  unsigned long sum_b;
  unsigned long sum_in_r;
  unsigned long sum_in_g;
  unsigned long sum_in_b;
  unsigned long sum_out_r;
  unsigned long sum_out_g;
  unsigned long sum_out_b;

  unsigned int wm = maxX - minX - 1;
  unsigned int hm = maxY - minY - 1;
  unsigned int w4 = w * 4;
  unsigned int div = (radius * 2) + 1;
  unsigned int mul_sum = stackblur_mul[radius];
  unsigned char shr_sum = stackblur_shr[radius];
  unsigned char stack[div * 3];

  {
    for (y = minY; y < maxY; y++) {
      sum_r = sum_g = sum_b = sum_in_r = sum_in_g = sum_in_b = sum_out_r =
          sum_out_g = sum_out_b = 0;

      src_ptr = src + w4 * y + (minX * 4);  // start of line (0,y)

      for (i = 0; i <= radius; i++) {
        stack_ptr = &stack[3 * i];
        stack_ptr[0] = src_ptr[0];
        stack_ptr[1] = src_ptr[1];
        stack_ptr[2] = src_ptr[2];
        sum_r += src_ptr[0] * (i + 1);
        sum_g += src_ptr[1] * (i + 1);
        sum_b += src_ptr[2] * (i + 1);
        sum_out_r += src_ptr[0];
        sum_out_g += src_ptr[1];
        sum_out_b += src_ptr[2];
      }

      for (i = 1; i <= radius; i++) {
        if (i <= wm) src_ptr += 4;
        stack_ptr = &stack[3 * (i + radius)];
        stack_ptr[0] = src_ptr[0];
        stack_ptr[1] = src_ptr[1];
        stack_ptr[2] = src_ptr[2];
        sum_r += src_ptr[0] * (radius + 1 - i);
        sum_g += src_ptr[1] * (radius + 1 - i);
        sum_b += src_ptr[2] * (radius + 1 - i);
        sum_in_r += src_ptr[0];
        sum_in_g += src_ptr[1];
        sum_in_b += src_ptr[2];
      }

      sp = radius;
      xp = radius;
      if (xp > wm) xp = wm;
      src_ptr = src + 4 * (xp + y * w) + (minX * 4);  //   img.pix_ptr(xp, y);
      dst_ptr = src + y * w4 + (minX * 4);            // img.pix_ptr(0, y);
      for (x = minX; x < maxX; x++) {
        unsigned int alpha = dst_ptr[3];
        dst_ptr[0] = clamp((sum_r * mul_sum) >> shr_sum, 0, alpha);
        dst_ptr[1] = clamp((sum_g * mul_sum) >> shr_sum, 0, alpha);
        dst_ptr[2] = clamp((sum_b * mul_sum) >> shr_sum, 0, alpha);
        dst_ptr += 4;

        sum_r -= sum_out_r;
        sum_g -= sum_out_g;
        sum_b -= sum_out_b;

        stack_start = sp + div - radius;
        if (stack_start >= div) stack_start -= div;
        stack_ptr = &stack[3 * stack_start];

        sum_out_r -= stack_ptr[0];
        sum_out_g -= stack_ptr[1];
        sum_out_b -= stack_ptr[2];

        if (xp < wm) {
          src_ptr += 4;
          ++xp;
        }

        stack_ptr[0] = src_ptr[0];
        stack_ptr[1] = src_ptr[1];
        stack_ptr[2] = src_ptr[2];

        sum_in_r += src_ptr[0];
        sum_in_g += src_ptr[1];
        sum_in_b += src_ptr[2];
        sum_r += sum_in_r;
        sum_g += sum_in_g;
        sum_b += sum_in_b;

        ++sp;
        if (sp >= div) sp = 0;
        stack_ptr = &stack[sp * 3];

        sum_out_r += stack_ptr[0];
        sum_out_g += stack_ptr[1];
        sum_out_b += stack_ptr[2];
        sum_in_r -= stack_ptr[0];
        sum_in_g -= stack_ptr[1];
        sum_in_b -= stack_ptr[2];
      }
    }
  }

  {
    for (x = minX; x < maxX; x++) {
      sum_r = sum_g = sum_b = sum_in_r = sum_in_g = sum_in_b = sum_out_r =
          sum_out_g = sum_out_b = 0;

      src_ptr = src + 4 * x + minY * w4;  // x,0
      for (i = 0; i <= radius; i++) {
        stack_ptr = &stack[i * 3];
        stack_ptr[0] = src_ptr[0];
        stack_ptr[1] = src_ptr[1];
        stack_ptr[2] = src_ptr[2];
        sum_r += src_ptr[0] * (i + 1);
        sum_g += src_ptr[1] * (i + 1);
        sum_b += src_ptr[2] * (i + 1);
        sum_out_r += src_ptr[0];
        sum_out_g += src_ptr[1];
        sum_out_b += src_ptr[2];
      }
      for (i = 1; i <= radius; i++) {
        if (i <= hm) src_ptr += w4;  // +stride

        stack_ptr = &stack[3 * (i + radius)];
        stack_ptr[0] = src_ptr[0];
        stack_ptr[1] = src_ptr[1];
        stack_ptr[2] = src_ptr[2];
        sum_r += src_ptr[0] * (radius + 1 - i);
        sum_g += src_ptr[1] * (radius + 1 - i);
        sum_b += src_ptr[2] * (radius + 1 - i);
        sum_in_r += src_ptr[0];
        sum_in_g += src_ptr[1];
        sum_in_b += src_ptr[2];
      }

      sp = radius;
      yp = radius;
      if (yp > hm) yp = hm;
      src_ptr = src + 4 * (x + yp * w) + minY * w4;  // img.pix_ptr(x, yp);
      dst_ptr = src + 4 * x + minY * w4;
      // img.pix_ptr(x, 0);
      for (y = minY; y < maxY; y++) {
        unsigned int alpha = dst_ptr[3];
        dst_ptr[0] = clamp((sum_r * mul_sum) >> shr_sum, 0, alpha);
        dst_ptr[1] = clamp((sum_g * mul_sum) >> shr_sum, 0, alpha);
        dst_ptr[2] = clamp((sum_b * mul_sum) >> shr_sum, 0, alpha);
        dst_ptr += w4;

        sum_r -= sum_out_r;
        sum_g -= sum_out_g;
        sum_b -= sum_out_b;

        stack_start = sp + div - radius;
        if (stack_start >= div) stack_start -= div;
        stack_ptr = &stack[3 * stack_start];

        sum_out_r -= stack_ptr[0];
        sum_out_g -= stack_ptr[1];
        sum_out_b -= stack_ptr[2];

        if (yp < hm) {
          src_ptr += w4;  // stride
          ++yp;
        }

        stack_ptr[0] = src_ptr[0];
        stack_ptr[1] = src_ptr[1];
        stack_ptr[2] = src_ptr[2];

        sum_in_r += src_ptr[0];
        sum_in_g += src_ptr[1];
        sum_in_b += src_ptr[2];
        sum_r += sum_in_r;
        sum_g += sum_in_g;
        sum_b += sum_in_b;

        ++sp;
        if (sp >= div) sp = 0;
        stack_ptr = &stack[sp * 3];

        sum_out_r += stack_ptr[0];
        sum_out_g += stack_ptr[1];
        sum_out_b += stack_ptr[2];
        sum_in_r -= stack_ptr[0];
        sum_in_g -= stack_ptr[1];
        sum_in_b -= stack_ptr[2];
      }
    }
  }
}




static long frame = 0;
void lumen::context::compose(void) {
  frame++;

  // do nothing if nothing has changed
  if (dirty_regions.size() == 0)

  {
    printf("nothing to do...\n");
    return;
  }

  // make a tmp bitmap
  gfx::bitmap b(screen.width(), screen.height(), screen.buffer());

  // this scribe is the "compositor scribe", used by everyone in this function
  gfx::scribe scribe(b);


  bool draw_mouse = screen.mouse_moved;
  // clear that information
  if (draw_mouse) screen.mouse_moved = false;

  for (auto &r : dirty_regions) {
    if (r.intersects(screen.mouse_rect())) draw_mouse = true;
    // scribe.fill_rect(r, 0x6261a1);
		scribe.fill_rect(r, 0x313032);
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
    // scribe.draw_pixel(screen.mouse_pos.x(), screen.mouse_pos.y(), 0);
    // scribe.draw_frame(screen.mouse_rect(), 0xFF8888);
    screen.draw_mouse();
  }


#if 1

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
#else

  uint32_t *to_ptr = screen.front_buffer;
  uint32_t *from_ptr = screen.back_buffer;
  memcpy(to_ptr, from_ptr, screen.width() * screen.height() * sizeof(uint32_t));
#endif


  compose_timer->stop();

  // printf("took %zu\n", tsc() - start);
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


long lumen::guest::send_raw(int type, int id, void *payload,
                            size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = id;
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy(msg + 1, payload, payloadsize);

  auto w = connection->send((void *)msg, msgsize, MSG_DONTWAIT);

  free(msg);
  return w;
}
