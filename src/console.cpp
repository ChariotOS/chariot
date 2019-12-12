#include <console.h>
#include <dev/driver.h>
#include <dev/serial.h>
#include <fifo_buf.h>
#include <lock.h>
#include <module.h>
#include <printk.h>
#include <util.h>

#include "drivers/majors.h"

// Control-x
#define C(x) ((x) - '@')

// if the console is in line buffer mode, it writes to here on input and flushes
// on '\n' or when the buffer fills up.
// TODO: maybe not flush when it fills up and allocate a new buffer? Not really
//       sure how it should work, and maybe look into how linux does it
#define LINEBUF_SIZE 4096
static bool buffer_input = false;
static bool echo = true;
static int line_len = 0;
static char line_buffer[LINEBUF_SIZE];
static mutex_lock cons_input_lock;

// the console fifo is defined globally
static fifo_buf console_fifo;

static void consputc(int c) {
  if (c == CONS_DEL) {
    serial_send(COM1, '\b');
    serial_send(COM1, ' ');
    serial_send(COM1, '\b');
  } else
    serial_send(COM1, c);
  // cgaputc(c);
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
    /*
    case C('B'):
      buffer_input = !buffer_input;
      printk("\nconsole buffering %s\n", buffer_input ? "on" : "off");
      return true;
    case C('E'):
      echo = !echo;
      printk("\nconsole echoing %s\n", buffer_input ? "on" : "off");
      return true;
      */
  }
  return false;
}

void console::feed(size_t sz, char *buf) {
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

struct console_driver : public dev::driver {
  console_driver() { dev::register_name("console", MAJOR_CONSOLE, 0); };
  virtual ~console_driver() {}

  // read from the fifo
  ssize_t read(minor_t minor, fs::filedesc &fd, void *buf, size_t sz) {
    if (minor != 0) return -1;
    return console_fifo.read(buf, sz);
  };

  // write is just a consputc() wrapper
  ssize_t write(minor_t minor, fs::filedesc &fd, const void *vbuf, size_t sz) {
    if (minor != 0) return -1;
    auto *buf = (const char *)vbuf;
    for (size_t i = 0; i < sz; i++) consputc(buf[i]);
    return sz;
  }
};

static void console_init(void) {
  dev::register_driver(MAJOR_CONSOLE, make_unique<console_driver>());
  //
}
module_init("console", console_init);
