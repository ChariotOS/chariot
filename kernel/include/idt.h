#ifndef __IDT__
#define __IDT__

#include <asm.h>
#include <types.h>

#define NUM_IDT_ENTRIES 256
#define NUM_EXCEPTIONS 32

struct gate_desc64 {
  u16 offset_1;  // offset bits 0..15
  u16 selector;  // a code segment selector in GDT or LDT
  u8 ist;  // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
  u8 type_attr;  // type and attributes
  u16 offset_2;  // offset bits 16..31
  u32 offset_3;  // offset bits 32..63
  u32 zero;      // reserved
};

struct idt_desc {
  uint16_t size;
  uint64_t base_addr;
} __packed;

void init_idt(void);

#endif
