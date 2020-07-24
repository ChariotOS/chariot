/* vim: tabstop=4 shiftwidth=4 noexpandtab
 */

OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64",
              "elf64-x86-64")

ENTRY(_start)

SECTIONS
{

	. = 0x10000;

	.text : {
		*(.text)
	}

	.rodata ALIGN(4K) : {

		__init_array_start = .;
        KEEP (*(.ctors))
		KEEP (*(.init_array*))
		__init_array_end = .;


		__fini_array_start = .;
        KEEP (*(.dtors))
		KEEP (*(.fini_array*))
		__fini_array_end = .;


        *(.rodata)
    }


	/*
	 * .bss not being last means it doesn't make filesz < memsz
	 * and that my crappy elf loader can work :)
	 * The downside is obviously that .bss needs to exist on the filesystem...
	 *
	 * TODO: make a better elf loader :)
	 */
	.data : { *(.data) }


	/**
	 * we don't handle bss very well in the kernel, so it needs to be
	 * seperate somehow from the rest of the sections
	 * */
	. += 4096;
	. = ALIGN(4096);

	.bss : { *(.bss) }
}


