#include <ui/application.h>
// #include "terminalview.h"
#include <ck/dir.h>
#include <gfx/color.h>
#include <gfx/font.h>
#include <gfx/scribe.h>

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


struct terminalview : public ui::view {
  int rows, cols;

  int cw, ch;  // char width and height

  int mouse_x, mouse_y;

  ck::file font_file;
  ck::unique_ptr<ck::file::mapping> file_mapping;


  ck::ref<gfx::font> font;

  FT_Library library; /* handle to library     */
  FT_Face face;       /* handle to face object */

  int lineheight = 14;

 public:
  terminalview() {
    font = gfx::font::get("Source Code Pro");
    /* Take some measurements on the font */
    font->with_line_height(lineheight, [this]() {
      cw = font->width('#');
      ch = font->line_height();
    });

    set_foreground(0xFFFFFF);
    set_background(0x000000);
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



  virtual void mounted(void) override { handle_resize(); }


  virtual void on_mouse_move(ui::mouse_event &ev) override {
    mouse_x = ev.x;
    mouse_y = ev.y;
    /*  */
    window()->resize(max(mouse_x * 2, 80), max(mouse_y * 2, 80));
    // handle_resize();
    // repaint();
  }



  virtual void paint_event(void) override {
    handle_resize();
    auto s = get_scribe();

    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        uint32_t c = 'A' + (rand() % ('z' - 'A'));
        draw_char(s, c, x, y, get_foreground(), get_background());
      }
    }
  }
};



int main() {
  ui::application app;

  ui::window *win = app.new_window("Terminal", 40, 40);
  auto &v = win->set_view<terminalview>();

  win->resize(80 * v.cw, 24 * v.ch);
  v.handle_resize();

  app.start();
  return 0;

#if 0
  const char *cmd[] = {"/bin/echo", "hello, world", NULL};
  int ret = sysbind_execve(cmd[0], cmd, environ);
  perror("exec");
  exit(ret);

  int ptmxfd = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

  ck::eventloop ev;
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

    ev.start();
  } else {
    ck::file ptmx(ptmxfd);

    // send input
    ck::in.on_read(fn() {
      int c = getchar();
      if (c == EOF) return;
      ptmx.write(&c, 1);
    });


    // echo
    ptmx.on_read(fn() {
      char buf[512];
      auto n = ptmx.read(buf, 512);
      if (n < 0) {
        perror("ptmx read");
        return;
      }
      ck::out.write(buf, n);
    });

    ev.start();
  }
#endif
  return 0;
}
