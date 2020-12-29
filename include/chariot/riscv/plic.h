#pragma once

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
		void hart_init(void);
		
		/* Acknowledge the current interrupt (eoi on x86) */
		void ack(void);
	}
}
