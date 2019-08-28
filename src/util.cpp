#include <asm.h>
#include <printk.h>
#include <util.h>

void hexdump(void *vdata, size_t len, int width) {
  int written = 0;

  auto *buf = static_cast<u8 *>(vdata);

  // TODO: dont use VLA
  u8 charbuf[width];

  bool trailing_newline = false;

  for_range(i, 0, len) {


    trailing_newline = false;
    if (written == 0) {
      printk("%p:", buf + i);
    }

    if (i % 2 == 0) printk(" ");

    printk("%02x", buf[i]);

    charbuf[written] = buf[i];

    written++;

    if (written == width) {
      written = 0;
      printk(" ");

      // write the char buffer
      for_range(j, 0, width) {
        char c = charbuf[j];
        printk("%c", (c < 0x20) || (c > 0x7e) ? '.' : c);
      }
      printk("\n");
    }
  }
  if (!trailing_newline) {
    printk("\n");
  }
}
