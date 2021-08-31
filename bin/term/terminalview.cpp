#include "terminalview.h"
#include <math.h>
#include <ck/time.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>


#define TERMINAL_IO_BUFFER_SIZE 4096

static int login_tty(int fd) {
  // setsid();

  close(0);
  close(1);
  close(2);

  int rc = dup2(fd, 0);
  if (rc < 0) return rc;
  rc = dup2(fd, 1);
  if (rc < 0) return -1;
  rc = dup2(fd, 2);
  if (rc < 0) return rc;
  rc = close(fd);
  if (rc < 0) return rc;
  rc = ioctl(0, TIOCSCTTY);
  if (rc < 0) return rc;
  return 0;
}


// this is basically dracula
static uint32_t terminal_color_theme[term::_COLOR_COUNT] = {
    [term::BLACK] = 0x000000,  // 0x21222C,
    [term::RED] = 0xFF5555,
    [term::GREEN] = 0x50FA7B,
    [term::YELLOW] = 0xF1FA8C,
    [term::BLUE] = 0xBD93F9,
    [term::MAGENTA] = 0xFF79C6,
    [term::CYAN] = 0x8BE9FD,
    [term::GREY] = 0xF8F8F2,
    [term::BRIGHT_BLACK] = 0x6272A4,
    [term::BRIGHT_RED] = 0xFF6E6E,
    [term::BRIGHT_GREEN] = 0x69FF94,
    [term::BRIGHT_YELLOW] = 0xFFFFA5,
    [term::BRIGHT_BLUE] = 0xD6ACFF,
    [term::BRIGHT_MAGENTA] = 0xFF92DF,
    [term::BRIGHT_CYAN] = 0xA4FFFF,
    [term::BRIGHT_GREY] = 0xFFFFFF,
    [term::FOREGROUND] = 0xF8F8F2,
    [term::BACKGROUND] = 0x000000,  // 0x21222C,
};


terminalview::terminalview(void) {
  cursor_blink_timer = ck::timer::make_interval(250, [this]() {
    this->blink();
    schedule_draw();
  });
  set_font("Source Code Pro Bold");
  set_foreground(terminal_color_theme[term::FOREGROUND]);
  set_background(terminal_color_theme[term::BACKGROUND]);
  set_font_size(14);
  update_charsz();

  ptmx.open("/dev/ptmx", O_RDWR | O_CLOEXEC);
  printf("tty: %s\n", ptsname(ptmx.fileno()));
  ptmx.on_read([this] { handle_read(); });

  shell_pid = fork();
  if (shell_pid == 0) {
    ck::file pts;
    pts.open(ptsname(ptmx.fileno()), "r+");
    int fd = pts.fileno();
    int res = setpgid(0, 0);

    int rc = login_tty(fd);
    pid_t pgid = getpgid(0);
    tcsetpgrp(0, pgid);

    // printf("rc=%d\n", rc);
    execl("/bin/sh", "/bin/sh", NULL);
  }
}

terminalview::~terminalview() {
  // TODO: kill the subprocess
}

// when you mount, replace the terminal with a new one
void terminalview::mounted(void) {
  m_term = ck::make_ref<term::terminal>(width() / charsz.width(), height() / charsz.height());
  m_term->surface().clear_all({});

  surface()->resize(80 * charsz.width(), 24 * charsz.height());
  //   handle_resize();
}

void terminalview::update_charsz() {
  auto font = get_font();
  font->with_line_height(get_font_size(), [this, &font]() {
    int max_width = 0;
    // crude, but works for now. *until unicode*
    for (uint32_t c = 0; c < 255; c++) {
      max_width = MAX(max_width, font->width(c));
    }
    printf("max_width: %d\n", max_width);
    charsz.set_width(max_width);
    charsz.set_height(font->line_height());
  });
}

void terminalview::draw_char(gfx::scribe &s, uint32_t cp, int x, int y, uint32_t fg, uint32_t bg) {
  if (bg != get_background()) {
    gfx::rect r;

    r.x = x * charsz.width();
    r.y = y * charsz.height();
    r.w = charsz.width();
    r.h = charsz.height();

    s.fill_rect(r, bg);
  }

  auto font = get_font();

  font->with_line_height(get_font_size(), [&]() {
    auto p = gfx::printer(
        s, *font, x * charsz.width(), y * charsz.height() + font->ascent(), charsz.width());
    p.set_color(fg);
    p.write(cp);
  });
}

void terminalview::paint_event(gfx::scribe &s) {
  // ck::time::logger l("oldterminalview::paint_event");
  handle_resize();

  // printf("since last frame: %d\n", m_chars_written_since_last_frame);
  m_chars_written_since_last_frame = 0;

  int cx = m_term->cursor().x;
  int cy = m_term->cursor().y;


  for (int y = 0; y < m_term->height(); y++) {
    for (int x = 0; x < m_term->width(); x++) {
      auto cell = m_term->surface().at(x, y);

      if (true || cell.dirty) {
        auto fg = terminal_color_theme[cell.attr.fg];
        auto bg = terminal_color_theme[cell.attr.bg];

        if (cx == x && cy == y && m_blink) {
          uint32_t tmp = fg;
          fg = bg;
          bg = tmp;
        }

        draw_char(s, cell.cp, x, y, fg, bg);
        // m_term->surface().undirty(x, y);
      }
    }
  }
}



void terminalview::mouse_event(ui::mouse_event &ev) {}


void terminalview::handle_read(void) {
  constexpr int bufsz = 4096 * 4;
  char buf[bufsz];
  while (1) {
    auto res = ptmx.read(buf, bufsz);

    if (!res.has_value() && ptmx.eof()) {
      // EOF!
      return;
    }

    auto sz = res.unwrap();
    if (sz > 0) {
      m_term->write(buf, sz);
      m_chars_written_since_last_frame += sz;
    } else {
      break;
    }
    schedule_draw();
    // update();

    break;
  }
}

void terminalview::on_keydown(ui::keydown_event &ev) {
  auto send_sequence = [&](auto sequence) { ptmx.write(sequence, strlen(sequence)); };

  switch (ev.code) {
    case key_delete:
      send_sequence("\e[3~");
      break;

    case key_end:
      send_sequence("\e[4~");
      break;

    case key_f1:
      send_sequence("\e[11~");
      break;

    case key_f2:
      send_sequence("\e[12~");
      break;

    case key_f3:
      send_sequence("\e[13~");
      break;

    case key_f4:
      send_sequence("\e[14~");
      break;

    case key_f5:
      send_sequence("\e[15~");
      break;

    case key_f6:
      send_sequence("\e[17~");
      break;

    case key_f7:
      send_sequence("\e[18~");
      break;

    case key_f8:
      send_sequence("\e[19~");
      break;

    case key_f9:
      send_sequence("\e[20~");
      break;

    case key_f10:
      send_sequence("\e[21~");
      break;

    case key_f11:
      send_sequence("\e[23~");
      break;

    case key_f12:
      send_sequence("\e[24~");
      break;

    case key_home:
      send_sequence("\e[1~");
      break;

    case key_insert:
      send_sequence("\e[2~");
      break;

    case key_pageup:
      send_sequence("\e[5~");
      break;

    case key_pagedown:
      send_sequence("\e[6~");
      break;

    case key_up:
      send_sequence("\e[A");
      break;

    case key_down:
      send_sequence("\e[B");
      break;

    case key_right:
      send_sequence("\e[C");
      break;

    case key_left:
      send_sequence("\e[D");
      break;

    default: {
      if (!ev.c) {
        return;
      }
      char c = ev.c;

      if (ev.flags & mod_ctrl) {
        if (c >= 'a' && c <= 'z') {
          c = c - 'a' + 1;
        } else if (c == '\\') {
          c = 0x1c;
        }
      }
      ptmx.write(&c, 1);

    }

    //   if (event->keyboard.codepoint != 0) {
    //     uint8_t buffer[5];
    //     int size = codepoint_to_utf8(event->keyboard.codepoint, buffer);
    //     _terminal_device.server.write(buffer, size);
    //     event->accepted = true;
    //   }
    break;
  }

  schedule_draw();
  // update();
}