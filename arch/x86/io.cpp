#include <asm.h>



uint8_t inb(off_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "dN"((uint16_t)port));
  return ret;
}

uint16_t inw(off_t port) {
  uint16_t ret;
  asm volatile("inw %1, %0" : "=a"(ret) : "dN"((uint16_t)port));
  return ret;
}

uint32_t inl(off_t port) {
  uint32_t ret;
  asm volatile("inl %1, %0" : "=a"(ret) : "dN"((uint16_t)port));
  return ret;
}

void outb(off_t port, uint8_t val) {
  asm volatile("outb %0, %1" ::"a"(val), "dN"((uint16_t)port));
}

void outw(off_t port, uint16_t val) {
  asm volatile("outw %0, %1" ::"a"(val), "dN"((uint16_t)port));
}

void outl(off_t port, uint32_t val) {
  asm volatile("outl %0, %1" ::"a"(val), "dN"((uint16_t)port));
}
