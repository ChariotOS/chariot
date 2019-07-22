/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

ENTRY(_start)

SECTIONS
{

  . = 0x1000;

	low_kern_start = .;

	.boot : {
		*(.multiboot)
		_loadStart = .;
		*(.boot)
		. = ALIGN(4096);

		p4_table = .;
		. += 0x1000;
		p3_table = .;
		. += 0x1000;
		p2_table = .;
		. += 0x1000;
		p1_table = .;
		. += 0x1000;

	}


	boot_stack_start = .;
	. += 0x4000;  /* we start out with a 4 pages of stack */
	boot_stack_end = .;



	low_kern_end = .;
	. = ALIGN(0x1000);

	. = 0x00100000;

	high_kern_start = .;

	.text : {
		*(.text*)
		*(.gnu.linkonce.t*)
	}



	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* Adjust the address for the data segment to the next page */
	. = ALIGN(0x1000);
	/* The data segment */
	.data : {
		*(.data)
	}

	.bss : {
		*(.bss)
	}


	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}

	. = ALIGN(0x1000);
	. += 0x1000;



	bitmap_metadata = .;
	. += 0x1000;
	bitmap_start = .;
	. += 0x1000 * 0x1000 * 8;
	bitmap_end = .;


	high_kern_end = .;
}
