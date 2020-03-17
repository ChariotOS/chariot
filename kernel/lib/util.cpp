#include <asm.h>
#include <printk.h>
#include <util.h>

#define C_RED 91
#define C_GREEN 92
#define C_YELLOW 93
#define C_BLUE 94
#define C_MAGENTA 95
#define C_CYAN 96

#define C_RESET 0
#define C_GRAY 90

static int current_color = 0;
static void set_color(int code) {
  if (code != current_color) {
    printk("\x1b[%dm", code);
    current_color = code;
  }
}

static void set_color_for(char c) {
  if (c >= 'A' && c <= 'z') {
    set_color(C_YELLOW);
  } else if (c >= '!' && c <= '~') {
    set_color(C_CYAN);
  } else if (c == '\n' || c == '\r') {
    set_color(C_GREEN);
  } else if (c == '\a' || c == '\b' || c == 0x1b || c == '\f' || c == '\n' ||
             c == '\r') {
    set_color(C_RED);
  } else {
    set_color(C_GRAY);
  }
}

#define HEX_META

static bool use_binary = false;

void hexdump(void *vbuf, size_t len, bool use_colors) {
#ifndef HEX_META
  use_colors = false;
#endif

  unsigned char *buf = (unsigned char *)vbuf;
  int w = use_binary ? 8 : 16;

  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;

    if (use_colors) {
      set_color(C_RESET);
      printk("|");
      set_color(C_GRAY);

      printk("%.8llx", i);

      set_color(C_RESET);
      printk("|");
    }
    for (int c = 0; c < w; c++) {
      if (c % 8 == 0) {
        if (use_colors) printk(" ");
      }
      if (i + c >= len) {
        printk("   ");
      } else {
        if (use_colors) set_color_for(line[c]);
        if (use_binary) {
          for (int j = 7; j >= 0; j--) {
            printk("%u", (line[c] >> j) & 1);
          }
          printk(" ");
        } else {
          printk("%02x ", line[c]);
        }
      }
    }

    if (use_colors) {
      set_color(C_RESET);
      printk("|");
      for (int c = 0; c < w; c++) {
        if (c != 0 && (c % 8 == 0)) {
          set_color(C_RESET);
          printk(" ");
        }

        if (i + c >= len) {
          printk(" ");
        } else {
          set_color_for(line[c]);

          printk("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
        }
      }
      set_color(C_RESET);
      printk("|\n");

    } else {
      printk("\n");
    }
  }
}
