section .data
;; storage for kernel parameters
extern __argc
extern __argv
extern __envp

section .text


extern libc_start

global _start
_start:

	mov [__argc], edi
	mov [__argv], esi
	mov [__envp], edx

	push rbp

	jmp libc_start

	pop rbp

	.invalid_loop:
		mov rax, [0x00]
		jmp .invalid_loop



# set the dynamic linker
section .interp
	db "/lib/ld.so", 0
