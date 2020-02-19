SECTIONS
{
	/* start on the second page */
  . = 0x40000;
  .text : { *(.text) }


	/* skip to the next page */
	. = ALIGN(4096);

	/*
	 * .bss not being last means it doesn't make filesz < memsz
	 * and that my crappy elf loader can work :)
	 * The downside is obviously that .bss needs to exist on the filesystem...
	 *
	 * TODO: make a better elf loader :)
	 */
  .bss : { *(.bss) }
  .data : { *(.data) }
}


