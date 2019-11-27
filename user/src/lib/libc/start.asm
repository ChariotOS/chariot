section .data
;; storage for kernel parameters
__argc dq 0
__argv dq 0
__envp dq 0

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
