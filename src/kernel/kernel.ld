/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

ENTRY(_start)

SECTIONS
{

	. = 0x1000;

	.boot : {
		*(.mbhdr)
		_loadStart = .;
		*(.boot)
		. = ALIGN(4096);
		pml4 = .;
		. += 0x1000;
		pdpt = .;
		. += 0x1000;
		pd = .;
		. += 0x1000;

		boot_stack_start = .;
		. += 0x200000;  /* we start out with a 2M stack */
		boot_stack_end = .;
	}


	.text ALIGN(0x1000) : {
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
}
