

OUTPUT_ARCH( "riscv" )

ENTRY( _start )


PAGE_SIZE  = 0x1000;
KERNEL_VMA = CONFIG_KERNEL_VIRTUAL_BASE;

SECTIONS
{
	/*
   * ensure that entry.S / _entry is at 0x80000000,
   * where qemu's -kernel jumps.
   */
  . = 0x80000000;

	.bootcode : {
    PROVIDE(_boot_start = .);
    *(.text.init)
    *(.boot.text* )
    PROVIDE(_boot_end = .);
	}


	.bootdata : {
    PROVIDE(_boot_data_start = .);
    *(.boot.data* )
    PROVIDE(_boot_data_end = .);
	}

	/* This is the boot stack */
	. = ALIGN(PAGE_SIZE);
	PROVIDE(_stack_start = .);
	. += CONFIG_RISCV_BOOTSTACK_SIZE * CONFIG_MAX_CPUS * PAGE_SIZE;
	PROVIDE(_stack = .);

	. += KERNEL_VMA;

  .text : AT(ADDR(.text) - KERNEL_VMA) {
    PROVIDE(_text_start = .);
		*(.text .text.*)
    PROVIDE(_text_end = .);
  }


	.init_array : AT(ADDR(.init_array) - KERNEL_VMA) {
		__init_array_start = .;
		KEEP (*(.init_array*))
    KEEP (*(.ctors))
		__init_array_end = .;
	}

	/* Include the list of termination functions sorted. */
	.fini_array :	AT(ADDR(.fini_array) - KERNEL_VMA) {
		__fini_array_start = .;
		KEEP (*(.fini_array*))
    KEEP (*(.dtors))
		__fini_array_end = .;
	}

  .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    PROVIDE(_rodata_end = .);
  } 

  .data : AT(ADDR(.data) - KERNEL_VMA) {
    . = ALIGN(4096);
    PROVIDE(_data_start = . - KERNEL_VMA);
    *(.sdata .sdata.*) *(.data .data.*)

		/* TODO: you don't need this. Restructure phys.cpp */
		. = ALIGN(PAGE_SIZE);
		phys_mem_scratch = .;
		. += 0x1000;

    PROVIDE(_data_end = . - KERNEL_VMA);
  }


  .bss : AT(ADDR(.bss) - KERNEL_VMA) {
    PROVIDE(_bss_start = . - KERNEL_VMA);
    *(.sbss .sbss.*) *(.bss .bss.*)
    PROVIDE(_bss_end = . - KERNEL_VMA);
	}

	/* This should be aligned, but you can't be too safe */
	. = ALIGN(PAGE_SIZE);
  PROVIDE(_kernel_end = .); 

}
