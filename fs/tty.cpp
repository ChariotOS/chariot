#include <fs/tty.h>

static int is_control(int c) { return c < ' ' || c == 0x7F; }

ref<struct tty> tty::create(void) {
  auto t = make_ref<tty>();

  /* Controlling and foreground processes are set to 0 by default */
  t->ct_proc = 0;
  t->fg_proc = 0;

  t->tios.c_iflag = ICRNL | BRKINT;
  t->tios.c_oflag = ONLCR | OPOST;
  t->tios.c_lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN;
  t->tios.c_cflag = CREAD | CS8;
  t->tios.c_cc[VEOF] = 4;      /* ^D */
  t->tios.c_cc[VEOL] = 0;      /* Not set */
  t->tios.c_cc[VERASE] = 0x7f; /* ^? */
  t->tios.c_cc[VINTR] = 3;     /* ^C */
  t->tios.c_cc[VKILL] = 21;    /* ^U */
  t->tios.c_cc[VMIN] = 1;
  t->tios.c_cc[VQUIT] = 28;  /* ^\ */
  t->tios.c_cc[VSTART] = 17; /* ^Q */
  t->tios.c_cc[VSTOP] = 19;  /* ^S */
  t->tios.c_cc[VSUSP] = 26;  /* ^Z */
  t->tios.c_cc[VTIME] = 0;
  t->tios.c_cc[VLNEXT] = 22;  /* ^V */
  t->tios.c_cc[VWERASE] = 23; /* ^W */

	t->next_is_verbatim = false;

  // TODO: store somewhere
  return t;
}

tty::~tty(void) {
	// TODO: remove from the storage
}

void tty::write_in(char c) { in.write(&c, 1, false); }

void tty::write_out(char c) { out.write(&c, 1, false); }

string tty::name(void) { return string::format("/dev/pts/%d", index); }

void tty::handle_input(char c) {
  if (next_is_verbatim) {
    next_is_verbatim = 0;
    canonical_buf += c;
    if (tios.c_lflag & ECHO) {
      if (is_control(c)) {
	output('^');
	output(('@' + c) % 128);
      } else {
	output(c);
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
	output('^');
	output(('@' + c) % 128);
	output('\n');
      }
      canonical_buf.clear();
      if (fg_proc) {
	printk("[tty] would send signal %d to group %d\n", sig, fg_proc);
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
      output('^');
      output('\010');
      return;
    }

    if (c == tios.c_cc[VKILL]) {
      canonical_buf.clear();
      if ((tios.c_lflag & ECHO) && !(tios.c_lflag & ECHOK)) {
	output('^');
	output(('@' + c) % 128);
      }
      return;
    }

    if (c == tios.c_cc[VERASE]) {
      /* Backspace */
      erase_one(tios.c_lflag & ECHOE);
      if ((tios.c_lflag & ECHO) && !(tios.c_lflag & ECHOE)) {
	output('^');
	output(('@' + c) % 128);
      }
      return;
    }

    if (c == tios.c_cc[VEOF]) {
      if (canonical_buf.size() > 0) {
	dump_input_buffer();
      } else {
	printk("[tty] interrupt input (^D)\n");
      }
      return;
    }

    canonical_buf.push(c);

    if (tios.c_lflag & ECHO) {
      if (is_control(c) && c != '\n') {
	output('^');
	output(('@' + c) % 128);
      } else {
	output(c);
      }
    }

    if (c == '\n' || (tios.c_cc[VEOL] && c == tios.c_cc[VEOL])) {
      if (!(tios.c_lflag & ECHO) && (tios.c_lflag & ECHONL)) {
	output(c);
      }
      dump_input_buffer();
      return;
    }
    return;
  } else if (tios.c_lflag & ECHO) {
    output(c);
  }

  write_in(c);
}

void tty::dump_input_buffer(void) {
  for (char c : canonical_buf) {
    write_in(c);
  }
  canonical_buf.clear();
}

void tty::erase_one(int erase) {
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

void tty::output(char c) {
  if (c == '\n' && (tios.c_oflag & ONLCR)) {
    c = '\n';
    write_out(c);
    c = '\r';
    write_out(c);
    return;
  }

  if (c == '\r' && (tios.c_oflag & ONLRET)) {
    return;
  }

  if (c >= 'a' && c <= 'z' && (tios.c_oflag & OLCUC)) {
    c = c + 'a' - 'A';
    write_out(c);
    return;
  }

  write_out(c);
}
