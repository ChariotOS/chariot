OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PAGE_SIZE  = 0x1000;
KERNEL_VMA = CONFIG_KERNEL_VIRTUAL_BASE;


SECTIONS
{
	. = 0x8000;

	.init_ap : {
		*(multiboot)
	}

	. = 1M;

  .init : {
    *(.initl)
    *(.init.text)
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

	.bss : AT(ADDR(.bss) - KERNEL_VMA) {
		*(.bss)
	}

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}

	. = ALIGN(4096);
	high_kern_end = . - KERNEL_VMA;
}
