#include <asm.h>
#include <cpu.h>
#include <desktop.h>
#include <dev/driver.h>
#include <errno.h>
#include <fb.h>
#include <kshell.h>
#include <module.h>
#include <sched.h>
#include <sem.h>
#include <syscall.h>
#include <vga.h>

#include "../majors.h"
#include "loadbmp.h"

static semaphore compositor_sem(0);
static struct {
  unsigned long long frames = 0;
  // default resolution
  int resx = 640, resy = 480;

  int mousex = 0, mousey = 0;
} globals;

static uint32_t *fb = NULL;

// just a list of the active windows, sorted by z index
static vec<ref<window>> windows;

static spinlock dirty_regions_lock;
static vec<struct rect> dirty_regions;

// mark a region (rectangle) as dirty so it is redrawn on the next composition
static void mark_dirty(const struct rect &r) {
  dirty_regions_lock.lock();
  dirty_regions.push(r);
  dirty_regions_lock.unlock();
}

static void compose(void) { compositor_sem.post(); }

#define COLOR_BORDER 0x444444
#define COLOR_BG 0x1E2020

static inline void set_pixel(int x, int y, int c, int alpha = 0xFF) {
  fb[x + y * globals.resx] = c;
}

static int window_size = 200;

static void draw_window(int x, int y, int w, int h) {
  auto r = rect(x, y, w, h);
  compositor::draw_rect_bordered(r, 0xFFFFFF, 0x000000, 1);

  r = rect(x, y, w, 22);
  compositor::draw_rect_bordered(r, 0xCCCCCC, 0x000000, 1);

  r.h = 1;
  r.x += 1;
  r.y += 1;
  r.w -= 2;
  compositor::draw_rect(r, 0xFFFFFF);

  r.w = 1;
  r.h = 20;
  compositor::draw_rect(r, 0xFFFFFF);
}

/**
 * the window server runs the compositor in a seperate thread, so it can
 * block and wait on new events.
 */
static int compositor_thread(void *) {
  fb = (uint32_t *)vga::get_fba();

  // TODO: this can fail
  // auto cursor = bitmap::load("/usr/res/icons/arrow.bmp");


  while (1) {
    // sys::usleep(1000 * 1000 / 60);
    compositor_sem.wait();
    scoped_lock dirty_regions_scopelock(dirty_regions_lock);

    fb = (uint32_t *)vga::get_fba();

    // this is really bad lol
    // memset(fb, 0x33, globals.resx * globals.resy * sizeof(uint32_t));

    draw_window(globals.mousex, globals.mousey, window_size, window_size);

		/*
    for (int y = 0; y < cursor.height(); y++) {
      for (int x = 0; x < cursor.width(); x++) {
				set_pixel(globals.mousex + x, globals.mousey + y, cursor.pix(x, y));
      }
    }
		*/

    // draw each window
    for (auto &window : windows) {
      printk("window %p\n", window.get());
    }

    globals.frames++;
  }
}

// TODO: dump to the process's stdout or whatever
static void desktop_debug_dump() { printk("frames: %llu\n", globals.frames); }

static void start(void) {
  struct ck_fb_info info;
  info.active = true;
  info.width = globals.resx;
  info.height = globals.resy;

  vga::configure(info);
}

static void set_res(int resx, int resy) {
  globals.resx = resx;
  globals.resy = resy;
  start();
}

static void stop(void) {
  struct ck_fb_info info;
  info.active = false;
  vga::configure(info);
}

static unsigned long desktop_shell(vec<string> &args, void *data, int dlen) {
  if (args.size() > 0) {
    if (args[0] == "composite" || args[0] == "c") {
      compositor_sem.post();
      return 0;
    }

    if (args[0] == "debug" || args[0] == "d") {
      desktop_debug_dump();
      return 0;
    }

    if (args[0] == "n") {
      printk("create window\n");
      return 0;
    }

    if (args[0] == "start") {
      start();
      return 0;
    }

    if (args[0] == "stop") {
      stop();
      return 0;
    }

    if (args[0] == "res") {
      if (args.size() != 3) return -EINVAL;
      int x = 0, y = 0;

      if (sscanf(args[1].get(), "%d", &x) != 1) return -EINVAL;
      if (sscanf(args[2].get(), "%d", &y) != 1) return -EINVAL;

      set_res(x, y);
      return 0;
    }

    return -EINVAL;
  }

  return 0;
}

static struct fs::file_operations desktop_ops = {};

static struct dev::driver_info desktop_driver {
  .name = "desktop", .type = DRIVER_CHAR, .major = MAJOR_MEM,

  .char_ops = &desktop_ops,
};

void desktop::init(void) {
  dev::register_driver(desktop_driver);

  // the main desktop file exists at minor = 0
  dev::register_name(desktop_driver, "desk", 0);

  kshell::add("desk", desktop_shell);
  sched::proc::create_kthread("[compositor]", compositor_thread);

  start();
  compose();
}

struct rect rect::intersect(const struct rect &other) const {
  int l = max(left(), other.left());
  int r = min(right(), other.right());
  int t = max(top(), other.top());
  int b = min(bottom(), other.bottom());

  if (l > r || t > b) {
    return rect(0, 0, 0, 0);
  }

  return rect(l, t, (r - l) + 0, (b - t) + 0);
}

struct rect compositor::screen_rect(void) {
  return rect(0, 0, globals.resx, globals.resy);
}
void compositor::draw_rect(const struct rect &r, int color) {
  auto bb = r.intersect(compositor::screen_rect());

  for (int y = bb.top(); y < bb.bottom(); y++) {
    for (int x = bb.left(); x < bb.right(); x++) {
      set_pixel(x, y, color);
    }
  }
}

void compositor::draw_rect_bordered(const struct rect &r, int bg, int border,
				    int w) {
  auto bb = r.intersect(compositor::screen_rect());

  for (int y = bb.top(); y < bb.bottom(); y++) {
    for (int x = bb.left(); x < bb.right(); x++) {
      int c = bg;
      if (y < r.top() + w || y >= r.bottom() - w || x < r.left() + w ||
	  x >= r.right() - w) {
	c = border;
      }
      set_pixel(x, y, c);
    }
  }
}

void compositor::draw_border(const struct rect &r, int color, int width) {
  struct rect pen = r;

  pen.h = width - 1;
  pen.w -= 1;

  // top
  draw_rect(pen, color);

  // bottom
  pen.y += r.h - width;
  draw_rect(pen, color);

  // left
  pen.h = r.h - width * 2;
  pen.w = width - 1;
  pen.x = r.x;
  pen.y = r.y + width;
  draw_rect(pen, color);

  // right
  pen.x += r.w - width;
  draw_rect(pen, color);
}

void desktop::mouse_input(struct mouse_packet &m) {
  if (m.buttons & MOUSE_SCROLL_UP) printk("scroll up\n");
  if (m.buttons & MOUSE_SCROLL_DOWN) printk("scroll down\n");
  if (m.buttons & MOUSE_LEFT_CLICK) printk("left click\n");
  if (m.buttons & MOUSE_RIGHT_CLICK) printk("right click\n");

  if (m.buttons & MOUSE_SCROLL_UP) window_size++;
  if (m.buttons & MOUSE_SCROLL_DOWN) window_size--;

  // printk("wnd: %d\n", window_size);
  /*
auto r = rect(globals.mousex, globals.mousey, 10, 10);
compositor::draw_rect(r, 0);
  */

  globals.mousex += m.dx;
  globals.mousey += m.dy;

  globals.mousex = max(0, min(globals.mousex, globals.resx));
  globals.mousey = max(0, min(globals.mousey, globals.resy));

  /*
r.x = globals.mousex;
r.y = globals.mousey;

int color = 0x333333;
if (m.buttons & MOUSE_LEFT_CLICK) color = 0xFF0000;
r.w = 5;
compositor::draw_rect(r, color);

color = 0x333333;
if (m.buttons & MOUSE_RIGHT_CLICK) color = 0xFF0000;
r.x += 5;
compositor::draw_rect(r, color);
  */

  compose();
}
