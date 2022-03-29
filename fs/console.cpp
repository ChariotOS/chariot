#include <console.h>
#include <dev/driver.h>
#include <fifo_buf.h>
#include <fs/tty.h>
#include <lock.h>
#include <module.h>
#include <printf.h>
#include <sched.h>
#include <signals.h>
#include <util.h>
#include <vga.h>
#include <kshell.h>
#include "../drivers/majors.h"

// Control-x
#define C(x) ((x) - '@')

static spinlock cons_input_lock;

// the console fifo is defined globally
static fifo_buf console_fifo;


static void consputc(int c, bool debug = false) {
#ifdef CONFIG_UART_CONSOLE
  if (unlikely(c == CONS_DEL)) {
    serial_send(1, '\b');
    serial_send(1, ' ');
    serial_send(1, '\b');
  } else {
    serial_send(1, c);
  }
#endif
  /* TODO: factor this somewhere else. Maybe a cool "console printers" subsystem? idk :^) */
#ifdef CONFIG_X86
  vga::putchar(c);
#endif
}

DECLARE_STUB_DRIVER("console", console_driver);


struct ConsoleNode : public TTYNode {
 public:
  ConsoleNode(void) : TTYNode(console_driver) {}
  virtual ~ConsoleNode() {}
  // write to the global console fifo
  virtual void write_in(char c) { console_fifo.write(&c, 1, false); }
  // write to the console using putc. We don't care about blocking
  virtual void write_out(char c, bool block = true) { consputc(c); }

  virtual ssize_t read(fs::File &, char *buf, size_t sz) { return console_fifo.read(buf, sz); }
  virtual int poll(fs::File &, int events, poll_table &pt) { return console_fifo.poll(pt) & events & AWAITFS_READ; }
};

ConsoleNode ctty;



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


/* Hitting f12 will enter the monitor */
#define MONITOR_ENTRY ("\x1b\x5b\x32\x34\x7e")
#define MON_ENTRY_SIZE (sizeof(MONITOR_ENTRY) - 1)

void console::feed(size_t sz, char *buf) {
  // short curcuit if the kernel shell is running.
  if (kshell::active()) {
    kshell::feed(sz, buf);
    return;
  }

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

#if 0

static ssize_t console_read(fs::File& fd, char* buf, size_t sz) {
  if (fd) {
    auto minor = fd.ino->minor;
    if (minor != 0) return -1;
    return console_fifo.read(buf, sz);
  }
  return -1;
}

spinlock console_write_lock;
static ssize_t console_write(fs::File& fd, const char* buf, size_t sz) {
  scoped_lock l(console_write_lock);
  if (fd) {
    auto minor = fd.ino->minor;
    if (minor != 0) return -1;
    for (size_t i = 0; i < sz; i++) {
      consputc(buf[i]);
    }
    return sz;
  }
  return -1;
}

#endif


static void console_init(void) { ctty.bind("console"); }
module_init("console", console_init);
