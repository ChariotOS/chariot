

OUTPUT_ARCH( "riscv" )

ENTRY( _start )


PAGE_SIZE  = 0x1000;

SECTIONS
{
	/*
   * ensure that entry.S / _entry is at 0x80000000,
   * where qemu's -kernel jumps.
   */
  . = 0x80000000;

  .text : {
    PROVIDE(_text_start = .);
    *(.text.init)
		*(.text .text.*)
    PROVIDE(_text_end = .);
  }


	.init_array : {
		__init_array_start = .;
		KEEP (*(.init_array*))
    KEEP (*(.ctors))
		__init_array_end = .;
	}

	/* Include the list of termination functions sorted. */
	.fini_array :	{
		__fini_array_start = .;
		KEEP (*(.fini_array*))
    KEEP (*(.dtors))
		__fini_array_end = .;
	}

  .rodata : {
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    PROVIDE(_rodata_end = .);
  } 

  .data : {
    . = ALIGN(4096);
    PROVIDE(_data_start = .);
    *(.sdata .sdata.*) *(.data .data.*)

		/* TODO: you don't need this. Restructure phys.cpp */
		. = ALIGN(PAGE_SIZE);
		phys_mem_scratch = .;
		. += 0x1000;

    PROVIDE(_data_end = .);
  }

  .bss :{
    PROVIDE(_bss_start = .);
    *(.sbss .sbss.*) *(.bss .bss.*)
    PROVIDE(_bss_end = .);
	}

	/* This is the boot stack */
	. = ALIGN(PAGE_SIZE);
	PROVIDE(_stack_start = .);
	. += 0x10000;
	PROVIDE(_stack = .);
	/* This should be aligned, but you can't be too safe */
	. = ALIGN(PAGE_SIZE);
  PROVIDE(_kernel_end = .); 

}
