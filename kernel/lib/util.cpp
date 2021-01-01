#include <asm.h>
#include <printk.h>
#include <util.h>

static int current_color = 0;
void set_color(int code) {
  if (code != current_color) {
    printk("\x1b[%dm", code);
    current_color = code;
  }
}

void set_color_for(char c, int fallback) {
  if (c >= 'A' && c <= 'z') {
    set_color(C_YELLOW);
  } else if (c >= '!' && c <= '~') {
    set_color(C_CYAN);
  } else if (c == '\n' || c == '\r') {
    set_color(C_GREEN);
  } else if (c == '\a' || c == '\b' || c == 0x1b || c == '\f' || c == '\n' || c == '\r') {
    set_color(C_RED);
  } else if ((unsigned char)c == 0xFF) {
    set_color(C_MAGENTA);
  } else {
    set_color(fallback);
  }
}

#define HEX_META

static bool use_binary = false;

void hexdump(void *vbuf, size_t len, bool use_colors) {
#ifndef HEX_META
  use_colors = false;
#endif

  unsigned awidth = 8;
  // if (len > 0xFFFFL) awidth = 8;

  unsigned char *buf = (unsigned char *)vbuf;
  int w = use_binary ? 8 : 16;

  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;

    if (use_colors) {
      set_color(C_RESET);
      printk("|");
      set_color(C_GRAY);

      printk("%.*llx", awidth, (off_t)vbuf + i);

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
          printk("%02X ", line[c]);
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


int isspace(int c) { return (c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == ' '); }


int isdigit(int c) { return (c >= '0' && c <= '9'); }

int atoi(const char *s) {
  int n = 0;
  int neg = 0;
  while (isspace(*s)) {
    s++;
  }
  switch (*s) {
    case '-':
      neg = 1;
      /* Fallthrough is intentional here */
    case '+':
      s++;
  }
  while (isdigit(*s)) {
    n = 10 * n - (*s++ - '0');
  }
  /* The sign order may look incorrect here but this is correct as n is
   * calculated as a negative number to avoid overflow on INT_MAX.
   */
  return neg ? n : -n;
}



uint64_t rand_seed;
void srand(unsigned s) { rand_seed = s - 1; }

int rand(void) {
  rand_seed = 6364136223846793005ULL * rand_seed + 1;

#ifdef CONFIG_64BIG
  return rand_seed >> 33;
#else
  /* hmm, gotta figure out the reasoning for this */
  return rand_seed;
#endif
}
