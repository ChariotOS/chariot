

org 0x1000 ;; start at the first page

[bits 64]


main:
	mov rax, 0
.lp:
	inc rax
	jmp .lp


; msg   db    'Hello, Friend!',0xa
; len   equ   $-msg

