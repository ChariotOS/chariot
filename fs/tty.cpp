#include <dev/driver.h>
#include <fs/tty.h>
#include <errno.h>
#include <termios.h>
#include <mm.h>

static int is_control(int c) { return c < ' ' || c == 0x7F; }


void TTYNode::reset(void) {
  /* Controlling and foreground processes are set to 0 by default */
  ct_proc = 0;
  fg_proc = 0;

  tios.c_iflag = ICRNL | BRKINT;
  tios.c_oflag = ONLCR | OPOST;
  tios.c_lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN;
  tios.c_cflag = CREAD | CS8;
  tios.c_cc[VEOF] = 4;      /* ^D */
  tios.c_cc[VEOL] = 0;      /* Not set */
  tios.c_cc[VERASE] = 0x7f; /* ^? */
  tios.c_cc[VINTR] = 3;     /* ^C */
  tios.c_cc[VKILL] = 21;    /* ^U */
  tios.c_cc[VMIN] = 1;
  tios.c_cc[VQUIT] = 28;  /* ^\ */
  tios.c_cc[VSTART] = 17; /* ^Q */
  tios.c_cc[VSTOP] = 19;  /* ^S */
  tios.c_cc[VSUSP] = 26;  /* ^Z */
  tios.c_cc[VTIME] = 0;
  tios.c_cc[VLNEXT] = 22;  /* ^V */
  tios.c_cc[VWERASE] = 23; /* ^W */
  next_is_verbatim = false;
}




TTYNode::TTYNode(dev::Driver &drv) : dev::CharDevice(drv) { reset(); }
TTYNode::~TTYNode(void) {
  // TODO: remove from the storage
}

/*
void tty::write_in(char c) { in.write(&c, 1, true); }
void tty::write_out(char c, bool block) { out.write(&c, 1, block); }
*/

ck::string TTYNode::name(void) { return ck::string::format("/dev/pts%d", index); }

int TTYNode::ioctl(fs::File &f, unsigned int cmd, off_t arg) {
  if (cmd == TIOCSPGRP) {
    fg_proc = arg;
    // printk(KERN_INFO "[tty %s] Setting PTY group to %d", name().get(), fg_proc);
    return 0;
  }

  if (cmd == TIOISATTY) return TIOISATTY_RET;


  if (cmd == TCSETSW) {
    // TODO: drain
    cmd = TCSETS;
  }

  if (cmd == TCSETSF) {
    // TODO: flush
    cmd = TCSETS;
  }

  if (cmd == TCSETS) {
    struct termios *t = (struct termios *)arg;

    if (!VALIDATE_RD(t, sizeof(*t))) return -EINVAL;
    this->tios = *t;
    return 0;
  }

  if (cmd == TCGETS) {
    struct termios *t = (struct termios *)arg;
    if (!VALIDATE_WR(t, sizeof(*t))) return -EINVAL;
    *t = this->tios;
    return 0;
  }

  return -EINVAL;
}

void TTYNode::handle_input(char c) {
  if (next_is_verbatim) {
    next_is_verbatim = 0;
    canonical_buf += c;
    if (tios.c_lflag & ECHO) {
      if (is_control(c)) {
        echo('^');
        echo(('@' + c) % 129);
      } else {
      }
    }
    return;
  }

  // now check if the char was a signal char
  if (tios.c_lflag & ISIG) {
    int sig = -1;
    if (c == tios.c_cc[VINTR]) {
      sig = SIGINT;
    } else if (c == tios.c_cc[VQUIT]) {
      sig = SIGQUIT;
    } else if (c == tios.c_cc[VSUSP]) {
      sig = SIGTSTP;
    }
    if (sig != -1) {
      if (tios.c_lflag & ECHO) {
        echo('^');
        echo(('@' + c) % 128);
        echo('\n');
      }
      canonical_buf.clear();
      if (fg_proc) {
        sched::proc::send_signal(-fg_proc, sig);
      } else {
        printk("[tty %s] would send signal %d but has no group. Currently %d\n", name().get(), sig, fg_proc);
      }
      return;
    }
  }

  /* ISTRIP: Strip eighth bit */
  if (tios.c_iflag & ISTRIP) {
    c &= 0x7F;
  }

  /* IGNCR: Ignore carriage return. */
  if ((tios.c_iflag & IGNCR) && c == '\r') {
    return;
  }

  if ((tios.c_iflag & INLCR) && c == '\n') {
    /* INLCR: Translate NL to CR. */
    c = '\r';
  } else if ((tios.c_iflag & ICRNL) && c == '\r') {
    /* ICRNL: Convert carriage return. */
    c = '\n';
  }

  if (tios.c_lflag & ICANON) {
    if (c == tios.c_cc[VLNEXT] && (tios.c_lflag & IEXTEN)) {
      next_is_verbatim = 1;
      echo('^');
      echo('\010');
      return;
    }

    if (c == tios.c_cc[VKILL]) {
      canonical_buf.clear();
      if ((tios.c_lflag & ECHO) && !(tios.c_lflag & ECHOK)) {
        echo('^');
        echo(('@' + c) % 128);
      }
      return;
    }

    if (c == tios.c_cc[VERASE]) {
      /* Backspace */
      erase_one(tios.c_lflag & ECHOE);
      if ((tios.c_lflag & ECHO) && !(tios.c_lflag & ECHOE)) {
        echo('^');
        echo(('@' + c) % 128);
      }
      return;
    }

    if (c == tios.c_cc[VEOF]) {
      if (canonical_buf.size() > 0) {
        dump_input_buffer();
      } else {
        printk("[tty] TODO: interrupt input (^D)\n");
      }
      return;
    }

    canonical_buf.push(c);

    if (tios.c_lflag & ECHO) {
      if (is_control(c) && c != '\n') {
        echo('^');
        echo(('@' + c) % 128);
      } else {
        echo(c);
      }
    }

    if (c == '\n' || (tios.c_cc[VEOL] && c == tios.c_cc[VEOL])) {
      if (!(tios.c_lflag & ECHO) && (tios.c_lflag & ECHONL)) {
        echo(c);
      }
      dump_input_buffer();
      return;
    }
    return;
  } else if (tios.c_lflag & ECHO) {
    echo(c);
  }

  write_in(c);
}

void TTYNode::dump_input_buffer(void) {
  for (char c : canonical_buf) {
    write_in(c);
  }
  canonical_buf.clear();
}

void TTYNode::erase_one(int erase) {
  if (canonical_buf.size() > 0) {
    /* How many do we backspace? */
    int vwidth = 1;

    char c = canonical_buf.pop();
    if (is_control(c)) {
      /* Erase ^@ */
      vwidth = 2;
    }
    if (tios.c_lflag & ECHO) {
      if (erase) {
        for (int i = 0; i < vwidth; ++i) {
          output('\010');
          output(' ');
          output('\010');
        }
      }
    }
  }
}

void TTYNode::output(char c, bool block) {
  if (c == '\n' && (tios.c_oflag & ONLCR)) {
    c = '\n';
    write_out(c, block);
    c = '\r';
    write_out(c, block);
    return;
  }

  if (c == '\r' && (tios.c_oflag & ONLRET)) {
    return;
  }

  if (c >= 'a' && c <= 'z' && (tios.c_oflag & OLCUC)) {
    c = c + 'a' - 'A';
    write_out(c, block);
    return;
  }

  write_out(c, block);
}



ssize_t TTYNode::write(fs::File &, const char *buf, size_t sz) {
  for (size_t i = 0; i < sz; i++) {
    write_out(buf[i]);
  }
  return sz;
}
