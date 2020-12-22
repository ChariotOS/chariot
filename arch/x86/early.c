/* This file is compiled to be a 32 bit executable in the init section (near the bootloader) */

int early(void) {
	unsigned long thing = CONFIG_KERNEL_VIRTUAL_BASE;
	return thing;
}
