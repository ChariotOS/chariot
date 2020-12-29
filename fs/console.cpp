#include <console.h>
#include <dev/driver.h>
#include <fifo_buf.h>
#include <fs/tty.h>
#include <lock.h>
#include <module.h>
#include <printk.h>
#include <sched.h>
#include <signals.h>
#include <util.h>
#include <vga.h>
#include "../drivers/majors.h"

// Control-x
#define C(x) ((x) - '@')

static spinlock cons_input_lock;

// the console fifo is defined globally
static fifo_buf console_fifo;

struct console_tty : public tty {
 public:
  virtual ~console_tty() {}

  // write to the global console fifo
  virtual void write_in(char c) { console_fifo.write(&c, 1, false); }

  // write to the console using putc. We don't care about blocking
  virtual void write_out(char c, bool block = true) { console::putc(c); }
};


static struct console_tty ctty;


static void consputc(int c, bool debug = false) {
  if (debug || true) {
    if (c == CONS_DEL) {
      serial_send(1, '\b');
      serial_send(1, ' ');
      serial_send(1, '\b');
    } else {
      serial_send(1, c);
    }
  }
	/* TODO: factor this somewhere else. Maybe a cool "console printers" subsystem? idk :^) */
#ifdef CONFIG_X86
  vga::putchar(c);
#endif
}

// return true if the char was special (like backspace)
static bool handle_special_input(char c) {
  switch (c) {
    case C('P'):
      sched::proc::dump_table();
      return true;
    case C('K'):
      // send a signal to init as a test
      sched::proc::send_signal(1, 12);
      return true;
  }
  return false;
}

void console::feed(size_t sz, char* buf) {
  // lock the input
  cons_input_lock.lock();

  for (int i = 0; i < sz; i++) {
    auto c = buf[i];
    if (!handle_special_input(c)) {
      ctty.handle_input(c);
    }
  }
  cons_input_lock.unlock();
}



void console::putc(char c, bool debug) { consputc(c, debug); }



static ssize_t console_read(fs::file& fd, char* buf, size_t sz) {
  if (fd) {
    auto minor = fd.ino->minor;
    if (minor != 0) return -1;
    return console_fifo.read(buf, sz);
  }
  return -1;
}

spinlock console_write_lock;
static ssize_t console_write(fs::file& fd, const char* buf, size_t sz) {
  scoped_lock l(console_write_lock);
  if (fd) {
    auto minor = fd.ino->minor;
    if (minor != 0) return -1;
    for (size_t i = 0; i < sz; i++) consputc(buf[i]);
    return sz;
  }
  return -1;
}


static int console_open(fs::file& fd) { return 0; }
static void console_close(fs::file& fd) { /* KINFO("[console] close!\n"); */
}


static int console_poll(fs::file& fd, int events, poll_table& pt) {
  return console_fifo.poll(pt) & events & AWAITFS_READ;
}

struct fs::file_operations console_ops = {
    .read = console_read,
    .write = console_write,
    .open = console_open,
    .close = console_close,
    .poll = console_poll,
};

static struct dev::driver_info console_driver_info {
  .name = "console", .type = DRIVER_CHAR, .major = MAJOR_CONSOLE,

  .char_ops = &console_ops,
};

static void console_init(void) {
  // ctty = new console_tty();
  dev::register_driver(console_driver_info);
  dev::register_name(console_driver_info, "console", 0);
}
module_init("console", console_init);
