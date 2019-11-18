[org 0x1000]
[bits 64]

main:
	mov eax, 6 ;; write

	mov rdi, 1
	mov rsi, msg
	mov rdx, len
	int 0x80 ;; syscall


	mov rdx, len

.loop:



	inc byte [rdx + msg]

	dec rdx
	cmp rdx, -1
	je .done
	jmp .loop

.done:
	jmp main



msg   db    'almkx932n okijp 9 aoisjp098ff 2',0xa
len   equ   $-msg





