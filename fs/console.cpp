#include <console.h>
#include <dev/driver.h>
#include <dev/serial.h>
#include <fifo_buf.h>
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

// if the console is in line buffer mode, it writes to here on input and flushes
// on '\n' or when the buffer fills up.
// TODO: maybe not flush when it fills up and allocate a new buffer? Not really
//       sure how it should work, and maybe look into how linux does it
#define LINEBUF_SIZE 4096

static bool buffer_input = false;
static bool echo = false;
static int line_len = 0;
static char line_buffer[LINEBUF_SIZE];
static spinlock cons_input_lock;

// the console fifo is defined globally
static fifo_buf console_fifo;


static void consputc(int c, bool debug = false) {
  if (debug || true) {
    if (c == CONS_DEL) {
      serial_send(COM1, '\b');
      serial_send(COM1, ' ');
      serial_send(COM1, '\b');
    } else {
      serial_send(COM1, c);
    }
  }

  vga::putchar(c);
}

static void flush(void) {
  if (line_len == 0) return;
  // write to the console's fifo
  console_fifo.write(line_buffer, line_len, false /* no block */);
  // clear out the line buffer
  memset(line_buffer, 0, LINEBUF_SIZE);
  line_len = 0;
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
      if (echo) consputc(c);

      // when buffered, DEL should delete a char in the line
      if (buffer_input && c == CONS_DEL) {
        if (line_len != 0) {
          line_buffer[--line_len] = '\0';
        }
      } else {
        // put the element in the line buffer
        line_buffer[line_len++] = c;
        if (c == '\n' || c == '\r') flush();
        if (line_len >= LINEBUF_SIZE) flush();
      }
    }
  }
  // flush the atomic input if we arent buffering
  if (!buffer_input) flush();
  cons_input_lock.unlock();
}

int console::getc(bool block) { return -1; }
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
static void console_close(fs::file& fd) { /* KINFO("[console] close!\n"); */ }


static int console_poll(fs::file &fd, int events) {
	return console_fifo.poll() & events;
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
  dev::register_driver(console_driver_info);
  dev::register_name(console_driver_info, "console", 0);
}
module_init("console", console_init);
