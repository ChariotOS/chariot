OUTPUT_ARCH( "riscv" )
ENTRY(_start)



PHDRS
{
    code PT_LOAD FLAGS(5);   /* PF_R|PF_X */
    rodata PT_LOAD FLAGS(4); /* PF_R */
    data PT_LOAD FLAGS(6);   /* PF_R|PF_W */
    bss PT_LOAD FLAGS(6);   /* PF_R|PF_W */
}


SECTIONS
{

	. = 0x10000;

	.text           :
	{
    *(.text .text*)
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)
	} :code

  . = ALIGN(CONSTANT(MAXPAGESIZE));

	.rodata : {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  } :rodata

	/* trick to force any extra sections to be emitted here */
	. = .;


	/*
	.tdata      : { *(.tdata .tdata.* .gnu.linkonce.td.*) } :data
	.tbss       : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
	*/

	. = ALIGN(4096);

	/*
	 * .bss not being last means it doesn't make filesz < memsz
	 * and that my crappy elf loader can work :)
	 * The downside is obviously that .bss needs to exist on the filesystem...
	 *
	 * TODO: make a better elf loader :)
	 */
	.data : {
		*(.data)


		__init_array_start = .;
    KEEP(*(.ctors .init_array))
		__init_array_end = .;

		__fini_array_start = .;
		KEEP(*(.dtors .fini_array))
		__fini_array_end = .;


		__resources_start = .;
		KEEP (*(.resources))
		__resources_end = .;

	} :data


	/**
	 * we don't handle bss very well in the kernel, so it needs to be
	 * seperate somehow from the rest of the sections
	 * */
	. += 4096;
	. = ALIGN(4096);

	.bss : { *(.bss) } :bss
}

