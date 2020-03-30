/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)




PAGE_SIZE  = 0x1000;
KERNEL_VMA = 0xffff880000000000;


SECTIONS
{

	. = 1M;
	_virt_start = . + KERNEL_VMA;

  .init : {
		*(.multiboot)
    *(.initl)

		/*
		 * the physical memory scratch page must live in the low memory,
		 * so we can map it freely in the 4k granularity region of virtual
		 * memory during initialization
		 */
		. = ALIGN(PAGE_SIZE);
		phys_mem_scratch = .;
		. += 0x1000;
  }



	. += KERNEL_VMA;


	high_kern_start = .;
	.text ALIGN(PAGE_SIZE) : AT(ADDR(.text) - KERNEL_VMA) {
		*(.init_high)
		*(.text*)
		*(.gnu.linkonce.t*)

		. = ALIGN(0x1000);
		idt_block = .;
		. += 0x1000;

		boot_cpu_local = .;
		. += 0x1000;
	}


	.init_array ALIGN(PAGE_SIZE) : AT(ADDR(.init_array) - KERNEL_VMA) {
		__init_array_start = .;
		KEEP (*(.init_array*))
		__init_array_end = .;
	}

	/* Include the list of termination functions sorted. */
	.fini_array ALIGN(PAGE_SIZE) :	AT(ADDR(.fini_array) - KERNEL_VMA) {
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array))
	}

	.rodata ALIGN(PAGE_SIZE) : AT(ADDR(.rodata) - KERNEL_VMA) {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* The data segment */
	.data : AT(ADDR(.data) - KERNEL_VMA) {
		*(.data)
	}

	.bss : AT(ADDR(.bss) - KERNEL_VMA) {
		*(.bss)
	}

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}

	_virt_end = .;
	high_kern_end = . - KERNEL_VMA;
}
