#include <debug.h>
#include <printk.h>
#include <util.h>

void debug::print_register(const char *name, reg_t value) {
  if (name != NULL) {
    printk("%4s: ", name);
  }

  char buf[sizeof(value) * 2 + 1];
  snprintk(buf, sizeof(buf), "%p", value);

  bool seen = false;
  for (int i = 0; i < sizeof(value) * 2; i++) {
    const char *p = buf + i;
    if (*p == '0' && !seen) {
      set_color(C_GRAY);
    } else {
      if (!seen) set_color(C_RESET);
      seen = true;

      /* If we are on an even boundary */
      if ((i & 1) == 0) {
        /* This is inefficient, but it lets me see ascii chars in the reg dump */
        char buf[3];
        buf[0] = p[0];
        buf[1] = p[1];
        buf[2] = 0;

        uint8_t c = 0;
        sscanf(buf, "%02x", &c);
        set_color_for(c, C_RESET);
      }
    }
    printk("%c", *p);
  }

  set_color(C_RESET);
}


ck::vec<off_t> debug::generate_backtrace(off_t virt_ebp) {
  ck::vec<off_t> backtrace;
  off_t bt[64];
  int count = arch_generate_backtrace(virt_ebp, bt, 64);
  for (int i = 0; i < count; i++)
    backtrace.push(bt[i]);
  return backtrace;
}