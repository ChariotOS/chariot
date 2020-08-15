#include <dev/driver.h>
#include <fs/tty.h>
#include <ioctl.h>
#include <module.h>
#include <syscall.h>
#include "../drivers/majors.h"

static int is_control(int c) { return c < ' ' || c == 0x7F; }

ref<struct tty> tty::create(void) {
  auto t = make_ref<tty>();


  t->reset();


  // TODO: store somewhere
  return t;
}


void tty::reset(void) {
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



tty::~tty(void) {
  // TODO: remove from the storage
}

void tty::write_in(char c) { in.write(&c, 1, true); }

void tty::write_out(char c) { out.write(&c, 1, true); }

string tty::name(void) { return string::format("/dev/pts%d", index); }

void tty::handle_input(char c) {
  if (next_is_verbatim) {
    next_is_verbatim = 0;
    canonical_buf += c;
    if (tios.c_lflag & ECHO) {
      if (is_control(c)) {
        output('^');
        output(('@' + c) % 128);
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
        output('^');
        output(('@' + c) % 128);
        output('\n');
      }
      canonical_buf.clear();
      if (fg_proc) {
        printk("[tty] would send signal %d to group %d\n", sig, fg_proc);
      } else {

        printk("[tty] would send signal %d but has no group\n", sig);
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



struct ptspriv {
  int id;
};

static spinlock pts_lock;
static map<int, ref<tty>> pts;
static ssize_t pts_read(fs::file &f, char *dst, size_t sz);
static ssize_t pts_write(fs::file &f, const char *dst, size_t sz);
static int pts_ioctl(fs::file &f, unsigned int cmd, off_t arg);
static ssize_t mx_read(fs::file &f, char *dst, size_t sz);
static ssize_t mx_write(fs::file &f, const char *dst, size_t sz);
static int pts_poll(fs::file &f, int events);

static struct fs::file_operations pts_ops = {
    .read = pts_read,
    .write = pts_write,
		.ioctl = pts_ioctl,
		.poll = pts_poll,
};



static struct dev::driver_info pts_driver {
  .name = "pts", .type = DRIVER_CHAR, .major = MAJOR_PTS, .char_ops = &pts_ops,
};


static auto getpts(int id) { return pts.get(id); }

static int allocate_pts() {
  pts_lock.lock();
  int i = 0;
  for (i = 0; true; i++) {
    // if there isn't a pts at this location, allocate one
    if (!pts.contains(i)) {
      pts[i] = tty::create();

      dev::register_name(pts_driver, string::format("vtty%d", i), i);
      break;
    } else {
      // if nobody is controlling this pts, return it
      if (!pts[i]->controlled) {
        break;
      }
    }
  }

  // take control of the pts
  pts[i]->lock.lock();
  pts[i]->controlled = true;
  pts[i]->lock.unlock();

  pts_lock.unlock();

  return i;
}


static int pts_ioctl(fs::file &f, unsigned int cmd, off_t arg) {
	return 0;
}

static void close_pts(int ptsid) {
  pts_lock.lock();

  auto pts = getpts(ptsid);
  pts->lock.lock();
  pts->controlled = false;
  pts->lock.unlock();


  pts_lock.unlock();
}

// #define TTY_DEBUG

#ifdef TTY_DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG fmt, ##args);
#else
#define DBG(fmt, args...)
#endif

static ssize_t pts_read(fs::file &f, char *dst, size_t sz) {
	DBG("pts_read\n");
  auto pts = getpts(f.ino->minor);

  return pts->in.read(dst, sz, true /* block? */);

  //
  if (pts->tios.c_lflag & ICANON) {
    return pts->in.read(dst, sz);
  } else {
    if (pts->tios.c_cc[VMIN] == 0) {
      return pts->in.read(dst, sz);
    } else {
      return pts->in.read(dst, min(pts->tios.c_cc[VMIN], sz));
    }
  }
  return -ENOSYS;
}

static ssize_t pts_write(fs::file &f, const char *dst, size_t sz) {
	DBG("pts_write\n");
  auto pts = getpts(f.ino->minor);

  for (size_t s = 0; s < sz; s++) {
    pts->output(dst[s]);
  }
  return sz;
}


static ssize_t mx_read(fs::file &f, char *dst, size_t sz) {
  auto pts = getpts(f.pflags);
  DBG("mx_read %d\n", sz);
  return pts->out.read(dst, sz);
}

static ssize_t mx_write(fs::file &f, const char *dst, size_t sz) {
  auto pts = getpts(f.pflags);
  DBG("mx_write %d\n", sz);

  for (size_t s = 0; s < sz; s++) {
    pts->handle_input(dst[s]);
  }

  return sz;
}



static int pts_poll(fs::file &f, int events) {
  auto pts = getpts(f.ino->minor);
  return pts->in.poll() & events & AWAITFS_READ;
}


static int mx_poll(fs::file &f, int events) {
	// DBG("mx_poll\n");
  auto pts = getpts(f.pflags);
  return pts->out.poll() & events & AWAITFS_READ;
}

static int mx_open(fs::file &f) {
  f.pflags = allocate_pts();
  DBG("mx open\n");
  return 0;
}

static void mx_close(fs::file &f) {
  DBG("mx close\n");
  close_pts(f.pflags);
}

static int mx_ioctl(fs::file &f, unsigned int cmd, off_t arg) {
  if (cmd == PTMX_GETPTSID) {
    return f.pflags;
  }
  return -EINVAL;
}




static struct fs::file_operations mx_ops = {
    .read = mx_read,
    .write = mx_write,
    .ioctl = mx_ioctl,
    .open = mx_open,
    .close = mx_close,
    .poll = mx_poll,
};


static struct dev::driver_info mx_driver {
  .name = "ptmx", .type = DRIVER_CHAR, .major = MAJOR_PTMX, .char_ops = &mx_ops,
};




static void tty_init(void) {
  dev::register_driver(mx_driver);
  dev::register_driver(pts_driver);
  //
  dev::register_name(mx_driver, "ptmx", 0);
}

module_init("tty", tty_init);
