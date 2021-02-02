

.global _start
.section .text
_start:
	# j _start
	/* Assume the stack is right :) */
	call libc_start

.invalid:
	j .invalid
	
