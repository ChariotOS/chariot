#include <ui/view.h>
#include <gfx/font.h>
#include "terminal.h"


class terminalview : public ui::view {
 public:
  terminalview(void);
  virtual ~terminalview(void);

  virtual void paint_event(gfx::scribe &s) override;
  virtual void on_keydown(ui::keydown_event &ev) override;
  virtual void mouse_event(ui::mouse_event &ev) override;
  virtual void mounted(void) override;

  void draw_char(gfx::scribe &s, uint32_t cp, int x, int y, uint32_t fg, uint32_t bg);


  int cols(void) { return width() / charsz.width(); }
  int rows(void) { return height() / charsz.height(); }

 private:
  void handle_resize(void) {
    if (m_term->width() != cols() || m_term->height() != rows()) {
      m_term->resize(width() / charsz.width(), height() / charsz.height());
    }
  }


  void blink(void) { m_blink = !m_blink; }
  void update_charsz(void);
  void handle_read(void);

  ck::ref<term::terminal> m_term;

  // how big each tile in the terminal should be
  gfx::isize charsz = {0, 0};
  bool m_blink = true;
  ck::ref<ck::timer> cursor_blink_timer;

  int m_scroll_offset = 0;

  int shell_pid = -1;

  ck::file ptmx;
};
