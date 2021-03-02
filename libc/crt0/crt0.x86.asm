section .data
;; storage for kernel parameters
; extern __argc
; extern __argv
; extern environ
section .text

extern libc_start
extern sysbind_sigreturn

global _start
_start:
	mov rbp, rsp
	push rbp
	push qword 0 ; we need to align the stack to 16 bytes for XMM operations

	call libc_start

	pop rbp


	.invalid_loop:
		mov rax, [0x00]
		jmp .invalid_loop

global __signal_return_callback
__signal_return_callback:
	mov rdi, rsp
	call sysbind_sigreturn

; set the dynamic linker
; section .interp
;   db "/bin/dynld", 0
