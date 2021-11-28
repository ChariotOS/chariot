OUTPUT_FORMAT("elf64-littleaarch64", "elf64-bigaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(_start)

PAGE_SIZE  = 0x1000;
KERNEL_VMA = 0; /* CONFIG_KERNEL_VIRTUAL_BASE; */


SECTIONS
{
	/** . = 0x40000000;**/
	. = 0x80000;

  .init : {
		KEEP(*(.init.text))
    *(.init*)

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
	}


	.init_array ALIGN(PAGE_SIZE) : AT(ADDR(.init_array) - KERNEL_VMA) {
		__init_array_start = .;
		KEEP (*(.init_array*))
    KEEP (*(.ctors))
		__init_array_end = .;
	}

	/* Include the list of termination functions sorted. */
	.fini_array ALIGN(PAGE_SIZE) :	AT(ADDR(.fini_array) - KERNEL_VMA) {
		__fini_array_start = .;
		KEEP (*(.fini_array*))
    KEEP (*(.dtors))
		__fini_array_end = .;
	}

	.rodata ALIGN(PAGE_SIZE) : AT(ADDR(.rodata) - KERNEL_VMA) {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* The data segment */
	.data : AT(ADDR(.data) - KERNEL_VMA) {
		*(.data)
	}


	__bss_start = .;
	.bss : AT(ADDR(.bss) - KERNEL_VMA) {
		*(.bss)
	}
	. = ALIGN(4096); /* align to page size */
	__bss_end = .;
	__bss_size = (__bss_end - __bss_start) >> 3;

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}

	. = ALIGN(4096);
	high_kern_end = . - KERNEL_VMA;
}
