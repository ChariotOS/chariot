[org 0x1000]
[bits 64]

main:

	mov eax, 6 ;; write

	mov rsi, 0
	jmp dumb


dumb:
	mov eax, 6 ;; write
	mov rdi, 1
	mov rdx, 32
	int 0x80

	add rsi, 32
	jmp dumb





msg   db    'hello, world',0xa
len   equ   $-msg





