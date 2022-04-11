#pragma once

#include <types.h>

/*
 * PLIC - platform level interrupt controller
 *
 * RISC-V harts can have both local and global interrupt sources, and only the global
 * sources are handled by the PLIC. That means that it handles things like keyboards,
 * UART data, disk activity, etc.
 *
 * Local interrupts, however, do not pass through the PLIC. Things like software interrupts
 * or the timer interrupts for each priv level.
 */

namespace rv {
  namespace plic {
    void discover(void);
    void hart_init(void);

    /* As the PLIC what interrupt we should handle */
    int claim(void);

    /* Tell the PLIC we've served an IRQ */
    void complete(int irq);

		uint32_t pending(void);

    void enable(int irq, int priority = 1);
    void disable(int irq);
  }  // namespace plic
}  // namespace rv
