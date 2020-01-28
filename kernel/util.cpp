#include <asm.h>
#include <printk.h>
#include <util.h>

void hexdump(void *vdata, size_t len, int width) {
  int written = 0;

  auto *buf = static_cast<u8 *>(vdata);

  // TODO: dont use VLA
  u8 charbuf[width];
  bool was_eq = false;
  int neq = 0;

  bool trailing_newline = false;

  for_range(i, 0, round_up(len, width)) {
    trailing_newline = false;

    if (written == 0 && i != 0) {
      auto *o = buf + i;

      bool eq = true;
      for (int j = 0; j < width; j++) {
        if (charbuf[j] != o[j]) {
          eq = false;
          break;
        }
      }

      if (eq) {
        if (!was_eq) {
          printk(" *");
        }
        was_eq = true;
        i += width-1;
        written = 0;
        neq++;
        continue;
      } else {
        if (was_eq) {
          printk(" %d\n", neq);
        }
        neq = 0;
        was_eq = false;
      }
    }

    if (written == 0) {
      printk("%p ", buf + i);
    }

    if (i % 2 == 0 && written != 0) printk(" ");

    if (i < len) {
      printk("%02x", buf[i]);
      charbuf[written] = buf[i];
    } else {
      printk("__");
      charbuf[written] = ' ';
    }

    written++;

    if (written == width) {
      written = 0;
      printk(" ");

      // write the char buffer
      for_range(j, 0, width) {
        char c = charbuf[j];
        printk("%c", (c < 0x20) || (c > 0x7e) ? '.' : c);
      }
      trailing_newline = true;
      printk("\n");
    }
  }
  if (!trailing_newline) {
    printk("\n");
  }
}
