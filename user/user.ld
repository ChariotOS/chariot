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

	.init_array : {
		__init_array_start = .;
		KEEP (*(.init_array*))
		__init_array_end = .;
	}

	/*
	 * .bss not being last means it doesn't make filesz < memsz
	 * and that my crappy elf loader can work :)
	 * The downside is obviously that .bss needs to exist on the filesystem...
	 *
	 * TODO: make a better elf loader :)
	 */
	.data : { *(.data) }

	. += 4096;
	. = ALIGN(4096);

	.bss : { *(.bss) }
}


