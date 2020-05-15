#include <asm.h>
#include <desktop.h>
#include <dev/driver.h>
#include <errno.h>
#include <fb.h>
#include <kshell.h>
#include <module.h>
#include <sched.h>
#include <sem.h>
#include <vga.h>
#include <cpu.h>

#include "../majors.h"

static semaphore compositor_sem(0);
static struct {
  unsigned long long frames = 0;
  // default resolution
  int resx = 1024, resy = 768;

  int mousex = 0, mousey = 0;
} globals;

static uint32_t *fb = NULL;

// just a list of the active windows, sorted by z index
static vec<ref<window>> windows;

static void compose(void) { compositor_sem.post(); }

#define COLOR_BORDER 0x444444
#define COLOR_BG 0x1E2020

/**
 * the window server runs the compositor in a seperate thread, so it can
 * block and wait on new events.
 */
static int compositor_thread(void *) {

  fb = (uint32_t *)vga::get_fba();

	auto r = compositor::screen_rect();
	r = rect(40, 40, 256, 128);
	compositor::draw_rect_bordered(r, COLOR_BG, COLOR_BORDER, 6);

	r = rect(70, 60, 128, 128);
	compositor::draw_rect_bordered(r, COLOR_BG, COLOR_BORDER, 1);

	r = rect(70, 60, 128, 24);
	compositor::draw_rect_bordered(r, COLOR_BG, COLOR_BORDER, 1);

  while (1) {
    compositor_sem.wait();
  	fb = (uint32_t *)vga::get_fba();
    // printk("work to be done on the compositor!\n");


    r = rect(globals.mousex, globals.mousey, 10, 10);
    compositor::draw_border(r, 0xFF00FF, 1);

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

static void desktop_init(void) {
  dev::register_driver(desktop_driver);

  // the main desktop file exists at minor = 0
  dev::register_name(desktop_driver, "desk", 0);

  kshell::add("desk", desktop_shell);
  sched::proc::create_kthread("[compositor]", compositor_thread);

  start();
  compose();
}
module_init("desktop", desktop_init);

static inline void set_pixel(int x, int y, int c, int alpha = 0xFF) {
  fb[x + y * globals.resx] = c;
}

struct rect rect::intersect(const struct rect &other) const {
  int l = max(left(), other.left());
  int r = min(right(), other.right());
  int t = max(top(), other.top());
  int b = min(bottom(), other.bottom());

  if (l > r || t > b) {
    return rect(0, 0, 0, 0);
  }

  return rect(l, t, (r - l) + 1, (b - t) + 1);
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

  for (int y = bb.top(); y < bb.bottom() - 1; y++) {
    for (int x = bb.left(); x < bb.right() - 1; x++) {
			int c = bg;
      if (y < r.top() + w || y >= r.bottom() - w ||
					x < r.left() + w || x >= r.right() - w) {
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


	auto r = rect(globals.mousex, globals.mousey, 10, 10);
	compositor::draw_rect(r, 0);
  globals.mousex += m.dx;
  globals.mousey += m.dy;

  globals.mousex = max(0, min(globals.mousex, globals.resx));
  globals.mousey = max(0, min(globals.mousey, globals.resy));

	r = rect(globals.mousex, globals.mousey, 10, 10);
	compositor::draw_border(r, 0xFF00FF, 1);
  compose();
}
