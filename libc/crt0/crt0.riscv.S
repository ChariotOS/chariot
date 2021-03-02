

.global _start
.section .text
_start:
	# j _start
	/* Assume the stack is right :) */
	call libc_start

.invalid:
	j .invalid
	

.global __signal_return_callback
__signal_return_callback:
	move a0, sp
	call sysbind_sigreturn
