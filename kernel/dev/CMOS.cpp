#include <dev/CMOS.h>
#include <asm.h>

u8 dev::CMOS::read(u8 index) {
  // select the CMOS address
  outb(0x70, index);
  // read from it
  return inb(0x71);
}
void dev::CMOS::write(u8 index, u8 value) {
  // select the CMOS address
  outb(0x70, index);
  // write to it
  outb(0x71, value);
}
