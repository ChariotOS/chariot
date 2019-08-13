/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

ENTRY(_start)

SECTIONS
{

  . = 1M;

	low_kern_start = .;

	.boot : {
		*(.mbhdr)
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

	. = ALIGN(0x1000);
	idt_block = .;
	. += 0x1000;


	/* address which is mapped when working with physical memory */
	phys_mem_scratch = .;
	. += 0x1000;

	boot_stack_start = .;
	. += 0x1000 * 10;
	boot_stack_end = .;




	low_kern_end = .;
	. = ALIGN(0x1000);

	high_kern_start = .;

	.text : {
		*(.text*)
		*(.gnu.linkonce.t*)
	}

	/* Include the list of initialization functions sorted. */
	.init_array : {
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
	}

	/* Include the list of termination functions sorted. */
	.fini_array :	{
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array))
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

	/* allocate pages for the "boot heap" which is
	 * used to allocate boot-time objects.
	 */
	bootheap_start = .;
	/* . += 0x1000 * 64; */


	high_kern_end = .;
}
