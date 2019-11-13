

org 0x1000 ;; start at the first page

[bits 64]

_start:
	mov edi, _start
.lp:
	inc edi

	mov rsi, rdi
	mov rdx, rdi
	mov r10, rdi
	mov r8, rdi

	mov eax, 7
	;; int 0x80
	int     0x80          ; call kernel
	jmp .lp


; msg   db    'Hello, Friend!',0xa
; len   equ   $-msg

