#include <console.h>
#include <lock.h>
#include <dev/serial.h>
#include <printk.h>

// Control-x
#define C(x) ((x) - '@')

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

void console::feed(size_t sz, char *buf) {
  // lock the input
  cons_input_lock.lock();
  for (int i = 0; i < sz; i++) {
    auto c = buf[i];
    switch (c) {
      case C('P'):  // Process listing.
        break;
        /*
      case C('U'):  // Kill line.
        while(input.e != input.w &&
              input.buf[(input.e-1) % INPUT_BUF] != '\n'){
          input.e--;
          consputc(BACKSPACE);
        }
        break;
        */
      case C('H'):
      case '\x7f':  // Backspace
                    // if(input.e != input.w){
        // input.e--;
        consputc(CONS_DEL);
        // }
        break;
      default:

        consputc(c);
        /*
        if(c != 0 && input.e-input.r < INPUT_BUF){
          c = (c == '\r') ? '\n' : c;
          input.buf[input.e++ % INPUT_BUF] = c;
          consputc(c);
          if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
            input.w = input.e;
            wakeup(&input.r);
          }
        }
        */
        break;
    }
  }
  cons_input_lock.unlock();
}

int console::getc(bool block) { return -1; }
