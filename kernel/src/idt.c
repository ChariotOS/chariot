#include <asm.h>
#include <idt.h>
#include <printk.h>
#include <types.h>

u64 idt_handler_table[NUM_IDT_ENTRIES];
u64 idt_state_table[NUM_IDT_ENTRIES];

struct gate_desc64 idt64[NUM_IDT_ENTRIES];

struct idt_desc idt_descriptor = {
    .base_addr = (uint64_t)&idt64,
    .size = (NUM_IDT_ENTRIES * 16) - 1,
};

#define EXCP_NAME 0
#define EXCP_MNEMONIC 1
const char* excp_codes[NUM_EXCEPTIONS][2] = {
    {"Divide By Zero", "#DE"},
    {"Debug", "#DB"},
    {"Non-maskable Interrupt", "N/A"},
    {"Breakpoint Exception", "#BP"},
    {"Overflow Exception", "#OF"},
    {"Bound Range Exceeded", "#BR"},
    {"Invalid Opcode", "#UD"},
    {"Device Not Available", "#NM"},
    {"Double Fault", "#DF"},
    {"Coproc Segment Overrun", "N/A"},
    {"Invalid TSS", "#TS"},
    {"Segment Not Present", "#NP"},
    {"Stack Segment Fault", "#SS"},
    {"General Protection Fault", "#GP"},
    {"Page Fault", "#PF"},
    {"Reserved", "N/A"},
    {"x86 FP Exception", "#MF"},
    {"Alignment Check", "#AC"},
    {"Machine Check", "#MC"},
    {"SIMD FP Exception", "#XM"},
    {"Virtualization Exception", "#VE"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Security Exception", "#SX"},
    {"Reserved", "N/A"},
};

int idt_assign_entry(u64 entry, u64 handler_addr, u64 state_addr) {
  if (entry >= NUM_IDT_ENTRIES) {
    printk("Assigning invalid IDT entry\n");
    return -1;
  }

  if (!handler_addr) {
    printk("attempt to assign null handler\n");
    return -1;
  }

  idt_handler_table[entry] = handler_addr;
  idt_state_table[entry] = state_addr;

  return 0;
}

void init_idt(void) {
  /*
  u32 i;

  u64 irq_start = (u64)&early_irq_handlers;
  u64 excp_start = (u64)&early_excp_handlers;

  // clear the IDT out
  memset(&idt64, 0, sizeof(struct gate_desc64) * NUM_IDT_ENTRIES);

  for (i = 0; i < NUM_EXCEPTIONS; i++) {
    set_intr_gate(idt64, i, (void*)(excp_start + i * 16));
    idt_assign_entry(i, (ulong_t)null_excp_handler, 0);
  }

  for (i = 32; i < NUM_IDT_ENTRIES; i++) {
    set_intr_gate(idt64, i, (void*)(irq_start + (i - 32) * 16));
    idt_assign_entry(i, (ulong_t)null_irq_handler, 0);
  }
  */
}
