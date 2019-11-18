[org 0x1000]
[bits 64]

main:

	mov r15, 0
.lp:
	inc r15

	mov eax, 7 ;; yield
	int 0x80
	jmp .lp

