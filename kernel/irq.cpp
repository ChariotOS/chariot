#include <arch.h>
#include <cpu.h>
#include <ck/map.h>
#include <sleep.h>
#include <ck/string.h>
#include <module.h>
#define NIRQS 256

struct irq_registration {
  const char *name;
  void *data;
  irq::handler handler;
};



#define IRQ_DEBUG(...) PFXLOG(BLU "IRQ", __VA_ARGS__)


static struct irq_registration irq_handlers[NIRQS];

// install and remove handlers
int irq::install(int irq, irq::handler handler, const char *name, void *data) {
  IRQ_DEBUG("[cpu %d] Register irq %d for '%s'. Data=%p\n", cpu::current().id, irq, name, data);
  if (irq < 0 || irq > NIRQS) return -1;
  irq_handlers[irq].name = name;
  irq_handlers[irq].data = data;
  irq_handlers[irq].handler = handler;
  arch::irq::enable(irq);
  return 0;
}

irq::handler irq::uninstall(int irq) {
  if (irq < 0 || irq > NIRQS) return nullptr;

  irq::handler old = irq_handlers[irq].handler;

  if (old != nullptr) {
    arch::irq::disable(irq);
    irq_handlers[irq].name = nullptr;
    irq_handlers[irq].handler = nullptr;
  }
  return old;
}

int irq::init(void) {
  if (!arch::irq::init()) return -1;
  return 0;
}

// cause an interrupt to be handled by the kernel's interrupt dispatcher
void irq::dispatch(int irq, reg_t *regs) {
  auto handler = irq_handlers[irq].handler;
  if (handler) {
    handler(irq, regs, irq_handlers[irq].data);
  } else {
		printk("Unhandled irq %d\n", irq);
	}
}

ksh_def("irqs", "display the interrupt handlers") {
  for (int i = 0; i < NIRQS; i++) {
    auto &reg = irq_handlers[i];
    if (reg.handler) {
      printk("%3d %s handler: %p, data: %p\n", i, reg.name, reg.handler, reg.data);
    }
  }
  return 0;
}
