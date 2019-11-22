org 0x1000
bits 64

global start

start:
	mov rax, 0
	inc rdx
	int 0x80
	jmp start
