

OUTPUT_ARCH( "riscv" )

ENTRY( _start )


PAGE_SIZE  = 0x1000;
KERNEL_VMA = CONFIG_KERNEL_VIRTUAL_BASE;

/*
 * ensure that entry.S / _entry is at 0x80000000,
 * where qemu's -kernel jumps.
 */
. = 0x80100000;


PHDRS
{
  boot PT_LOAD;
  text PT_LOAD;
  rodata PT_LOAD;
  data PT_LOAD;
}



SECTIONS
{

	.bootcode : {
    PROVIDE(_boot_start = .);
    *(.text.init)
    *(.boot.text* )
    PROVIDE(_boot_end = .);
	} :boot


	.bootdata : {
    PROVIDE(_boot_data_start = .);
    *(.boot.data* )

		PROVIDE(_boot_data_end = .);
	} :boot


	PROVIDE(_stack_start = .);
	. += CONFIG_RISCV_BOOTSTACK_SIZE * CONFIG_MAX_CPUS * PAGE_SIZE;
	PROVIDE(_stack = .);


	. += KERNEL_VMA;

  .text : AT(ADDR(.text) - KERNEL_VMA) ALIGN(4096) {
    PROVIDE(_text_start = .);
		*(.text .text.*)
    PROVIDE(_text_end = .);
  } :text

  .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    PROVIDE(_rodata_end = .);
  } :rodata

	.rela.dyn : {
		_rela_start = .;
		*(.rela)
		*(.rela.*)
		_rela_end = .;
	} :rodata

	.init_array : AT(ADDR(.init_array) - KERNEL_VMA) {
		__init_array_start = .;
		KEEP (*(.init_array*))
    KEEP (*(.ctors))
		__init_array_end = .;
	} :data

	/* Include the list of termination functions sorted. */
	.fini_array :	AT(ADDR(.fini_array) - KERNEL_VMA) {
		__fini_array_start = .;
		KEEP (*(.fini_array*))
    KEEP (*(.dtors))
		__fini_array_end = .;
	} :data


  .data : AT(ADDR(.data) - KERNEL_VMA) {
    PROVIDE(_data_start = . - KERNEL_VMA);
    *(.sdata .sdata.*) *(.data .data.*)

		_symtab_start = .;
		*(.symtab*)
		_symtab_end = .;

		_initrd_start = .;
		*(.initrd*)
		_initrd_end = .;

    PROVIDE(_data_end = . - KERNEL_VMA);
  } :data

	.strtab   0 : AT(ADDR(.strtab) - KERNEL_VMA) { *(.strtab) }
	.shstrtab 0 : AT(ADDR(.shstrtab) - KERNEL_VMA) { *(.shstrtab) }


  .bss : AT(ADDR(.bss) - KERNEL_VMA) {
    PROVIDE(_bss_start = . - KERNEL_VMA);
    *(.sbss .sbss.*) *(.bss .bss.*)
    PROVIDE(_bss_end = . - KERNEL_VMA);
	} :data

	/* This should be aligned, but you can't be too safe */
	. = ALIGN(PAGE_SIZE);
  PROVIDE(_kernel_end = .); 

	/DISCARD/ : {
		*(.discard)
		*(.discard.*)
		*(.interp .dynamic)
		*(.dynsym .dynstr .hash .gnu.hash)
		*(.eh_frame)
		*(.gnu.version*)
		*(.note*)
		*(.comment*)
	}

}
