#include <idt.h>
#include <module.h>
#include <pci.h>
#include <printk.h>

#define E1000_DEBUG

#ifdef E1000_DEBUG
#define INFO(fmt, args...) printk("[E1000] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

static void e1000_interrupt(int intr, trapframe *fr) {
  INFO("interrupt: err=%d\n", fr->err);
}

void e1000_init(void) {
  return;
  pci::device *e1000_dev = nullptr;
  pci::walk_devices([&](pci::device *dev) {
    if (dev->is_device(0x8086, 0x100e)) {
      e1000_dev = dev;
    }
  });

  if (e1000_dev != nullptr) {

    e1000_dev->enable_bus_mastering();

    u16 irq = e1000_dev->interrupt;
    printk("irq: %d\n", irq);
    interrupt_register(irq, e1000_interrupt);
  }

  printk("e1000 init\n");
}

module_init("e1000", e1000_init);
