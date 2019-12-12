#include <console.h>
#include <dev/driver.h>
#include <dev/serial.h>
#include <lock.h>
#include <module.h>
#include <printk.h>
#include "drivers/majors.h"
#include <util.h>

// Control-x
#define C(x) ((x) - '@')

// if the console is in line buffer mode, it writes to here on input and flushes
// on '\n' or when the buffer fills up.
// TODO: maybe not flush when it fills up and allocate a new buffer? Not really
//       sure how it should work, and maybe look into how linux does it
#define LINEBUF_SIZE 512

bool buffer_input = false;
bool echo = true;
int line_len = 0;
char line_buffer[LINEBUF_SIZE];

mutex_lock cons_input_lock;

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
  printk("console flush to fifo!\n");


  hexdump(line_buffer, line_len);
  // clear out the line buffer
  memset(line_buffer, 0, LINEBUF_SIZE);
  line_len = 0;
}

// return true if the char was special (like backspace)
static bool handle_special_input(char c) {
  switch (c) {
    case C('B'):
      printk("buffering %d\n", buffer_input);
      buffer_input = !buffer_input;
      return true;
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
      // put the element in the line buffer
      line_buffer[line_len++] = c;
      if (c == '\n' || c == '\r') flush();
      if (line_len >= LINEBUF_SIZE) flush();
    }
  }
  // flush the atomic input if we arent buffering
  if (!buffer_input) flush();
  cons_input_lock.unlock();
}

int console::getc(bool block) { return -1; }

class console_driver : public dev::driver {
 public:
  console_driver() { dev::register_name("console", MAJOR_CONSOLE, 0); };
  virtual ~console_driver() {}

  ssize_t read(minor_t minor, fs::filedesc &fd, void *buf, size_t sz) {
    if (minor != 0) return -1;

    memset(buf, 0x01, sz);
    return -1;
  };

  ssize_t write(minor_t minor, fs::filedesc &fd, const void *buf, size_t sz) {
    if (minor != 0) return -1;
    return -1;
  }
};

static void console_init(void) {
  dev::register_driver(MAJOR_CONSOLE, make_unique<console_driver>());

  //
}
module_init("console", console_init);
