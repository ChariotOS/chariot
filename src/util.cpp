#include <asm.h>
#include <printk.h>
#include <util.h>

static int roundUp(int numToRound, int multiple) {
  if (multiple == 0) return numToRound;

  int remainder = numToRound % multiple;
  if (remainder == 0) return numToRound;

  return numToRound + multiple - remainder;
}

void hexdump(void *vdata, size_t len, int width) {
  int written = 0;

  auto *buf = static_cast<u8 *>(vdata);

  // TODO: dont use VLA
  u8 charbuf[width];

  bool trailing_newline = false;

  for_range(i, 0, roundUp(len, width)) {
    trailing_newline = false;
    if (i % 2 == 0 && i != 0) printk(" ");

    if (i < len) {
      printk("%02x", buf[i]);
      charbuf[written] = buf[i];
    } else {
      printk("  ");
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
