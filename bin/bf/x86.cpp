#include "x86.h"

using namespace x86;

void x86::inst::encode(code &c) {
  if (has_rex) {
    c.bytes(rex);
  }
  for (int i = 0; i < opcode_size; i++) {
    c.bytes(opcode[i]);
  }
  if (has_modrm) {
    uint8_t m = 0x00;
    m |= (mod << 6);
    m |= (reg << 3);
    m |= (rm << 0);
    c.bytes(m);
  }
  for (int i = 0; i < disp_size; i++) {
    c.bytes(displacement[i]);
  }
  for (int i = 0; i < imm_size; i++) {
    c.bytes(immediate[i]);
  }
}

uint32_t x86::inst::size(void) {
  uint32_t sz = 0;

  // one byte for rex
  if (has_rex) sz += 1;
  sz += opcode_size;
  if (has_modrm) sz += 1;

  sz += disp_size;
  sz += imm_size;
  return sz;
}


void code::prologue() {
  *this << push(ebp);
  *this << mov(ebp, esp);
}


void code::epilogue() {
  *this << x86::pop(x86::ebp);
  *this << ret();
}
