#include <arch.h>
#include <cpu.h>
#include <map.h>
#include <string.h>

#define NIRQS 130

static irq::handler irq_handlers[NIRQS];

// install and remove handlers
int irq::install(int irq, irq::handler handler, const char *name) {
  if (irq < 0 || irq > NIRQS) return -1;
  irq_handlers[irq] = handler;
  arch::irq::enable(irq);
  return 0;
}

irq::handler irq::uninstall(int irq) {
  if (irq < 0 || irq > NIRQS) return nullptr;

  irq::handler old = irq_handlers[irq];

  if (old != nullptr) {
    arch::irq::disable(irq);
    irq_handlers[irq] = nullptr;
  }
  return old;

  return old;
}

int irq::init(void) {
  if (!arch::irq::init()) return -1;
  return 0;
}

// cause an interrupt to be handled by the kernel's interrupt dispatcher
void irq::dispatch(int irq, reg_t*regs) {
  // store the current register state in the CPU for introspection
  // if (cpu::in_thread()) cpu::thread()->trap_frame = regs;

  auto handler = irq_handlers[irq];
  if (handler != nullptr) {
    handler(irq, regs);
  }
}
