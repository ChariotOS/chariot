#pragma once

#include <chan.h>
#include <desktop_structs.h>  // userspace api structs
#include <lock.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <mouse_packet.h>

struct rect {
  // simple!
  int x, y, w, h;

  void draw(int color);

  inline rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}

  inline int left() const { return x; }
  inline int right() const { return x + w; }
  inline int top() const { return y; }
  inline int bottom() const { return y + h; }

  struct rect intersect(const struct rect &other) const;

  inline bool intersects(const rect &other) const {
    return left() <= other.right() && other.left() <= right() &&
	   top() <= other.bottom() && other.top() <= bottom();
  }
};

/**
 * every renderable *thing* is represented as a window in some kind
 * of global list in the window manager
 */
struct window : public refcounted<window> {
  // every window can exist on any workspace. Think "mac control center"
  int workspace = 0;

  // the z index of the window. 0 is in front, n > 0 is further back
  int zind = 0;

  // the bounds of the window, including the window decorations. The content
  // position and size is derived from this.
  struct rect bounds;

  // pending userspace desktop events. This is what is read by the user.
  chan<struct desktop_window_event> pending;

  rwlock rw;

  window(struct rect bounds);
};

namespace compositor {

struct rect screen_rect(void);
void draw_rect(const struct rect &r, int color);
void draw_rect_bordered(const struct rect &r, int bg, int border_color, int width = 1);
void draw_border(const struct rect &r, int border_color, int width = 1);
};  // namespace compositor


namespace desktop {
	void mouse_input(struct mouse_packet &m);
};
