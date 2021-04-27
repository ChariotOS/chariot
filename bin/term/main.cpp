#include <ui/application.h>
// #include "terminalview.h"
#include <unistd.h>
#include <fcntl.h>
#include <ck/dir.h>
#include <ck/timer.h>
#include <gfx/color.h>
#include <gfx/font.h>
#include <gfx/image.h>
#include <gfx/scribe.h>
#include <sys/sysbind.h>


extern const char **environ;

#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H



#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })




// #define LIGHT

#ifdef LIGHT

#define TERMINAL_BG 0xFFFFFFFF
#define TERMINAL_FG 0xFF000000
#define TERMINAL_BORDER 0xFF999999

#else

#define TERMINAL_BG 0xFF000000
#define TERMINAL_FG 0xFFFFFFFF
#define TERMINAL_BORDER 0xFF333333

#endif


struct terminalview : public ui::view {
  int rows, cols;

  int cw, ch;  // char width and height

  int mouse_x, mouse_y;
  bool clicked = false;

  ck::file font_file;
  ck::unique_ptr<ck::file::mapping> file_mapping;


  ck::ref<gfx::font> font;

  FT_Library library; /* handle to library     */
  FT_Face face;       /* handle to face object */

  int lineheight = 13;

  ck::ref<gfx::bitmap> emoji;

 public:
  terminalview() {
    font = gfx::font::get("Source Code Pro");
    /* Take some measurements on the font */
    font->with_line_height(lineheight, [this]() {
      cw = font->width('#');
      ch = font->line_height();
    });

    set_flex_grow(1.0);

    set_foreground(TERMINAL_FG);
    set_background(TERMINAL_BG);
  }


  void handle_resize(void) {
    rows = height() / ch;
    cols = width() / cw;
  }


  void draw_char(gfx::scribe &s, uint32_t cp, int x, int y, uint32_t fg, uint32_t bg) {
    gfx::rect r;
    r.x = x * cw;
    r.y = y * ch;
    r.w = cw;
    r.h = ch;

    if (r.contains(mouse_x, mouse_y)) {
      auto tmp = bg;
      bg = fg;
      fg = tmp;
    }

    s.fill_rect(r, bg);

    font->with_line_height(lineheight, [&]() {
      auto p = gfx::printer(s, *font, x * cw, y * ch + font->ascent(), cw);
      p.set_color(fg);
      p.write(cp);
    });
  }




  virtual void mounted(void) override {
    handle_resize();
  }


  virtual void mouse_event(ui::mouse_event &ev) override {
    mouse_x = ev.pos().x();
    mouse_y = ev.pos().y();
    clicked = ev.left;
    // surface()->resize(max(mouse_x * 2, 80), max(mouse_y * 2, 80));
    repaint();
  }



	virtual void on_keydown(ui::keydown_event &ev) override {
		printf("keypress %02x '%c'\n", ev.code, ev.c);
	}

  virtual void paint_event(void) override {
    handle_resize();
    auto s = get_scribe();

		/*
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        uint32_t c = 'A' + (rand() % ('z' - 'A'));
        draw_char(s, c, x, y, get_foreground(), clicked ? 0x00FF00 : get_background());
      }
    }
		*/


    s.draw_line_antialias(gfx::point(mouse_x, mouse_y), gfx::point(width() / 2, height() / 2),
                          0xFF0000);
  }
};




int main() {
  ui::application app;

  ui::window *win = app.new_window("Terminal", 256, 256);
  win->defer_invalidation(false);
  win->compositor_sync(false);


  win->set_theme(TERMINAL_BG, TERMINAL_FG, TERMINAL_BORDER);
  auto &v = win->set_view<terminalview>();

  win->resize(80 * v.cw, 24 * v.ch);
  v.handle_resize();

  /*
  double x = 0;
  auto t = ck::timer::make_interval(16, [&]() {
                  int dx = (rand() % 10) - 5;
                  int dy = (rand() % 10) - 5;
                  struct lumen::move_request movreq {
                          .id = win->id(),
                          .dx = dx, .dy = dy,
                  };

                  app.send_msg(LUMEN_MSG_MOVEREQ, movreq);
  });
  */

  app.start();
  return 0;

#if 1
  int ptmxfd = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

  int pid = sysbind_fork();
  if (pid == 0) {
    ck::file pts;
    pts.open(ptsname(ptmxfd), "r+");

    while (1) {
      char data[32];
      auto n = pts.read(data, 32);
      if (n < 0) continue;
      ck::hexdump(data, n);
    }

  } else {
    ck::file ptmx(ptmxfd);

    // send input
    ck::in.on_read([&]() {
      int c = getchar();
      if (c == EOF) return;
      ptmx.write(&c, 1);
    });


    // echo
    ptmx.on_read([&]() {
      char buf[512];
      auto n = ptmx.read(buf, 512);
      if (n < 0) {
        perror("ptmx read");
        return;
      }
      ck::out.write(buf, n);
    });

  }
#endif
  return 0;
}
