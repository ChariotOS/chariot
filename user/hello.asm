[global _start]
[org 0x1000] ;; start at the first page
[bits 64]

_start:
	mov     edx,len  ; message length
	mov     ecx,msg  ; message to write
	mov     ebx,1    ; file descriptor (stdout)
	mov     eax,4    ; system call number (sys_write)
	syscall          ; call kernel
	jmp _start

	mov eax, 1       ; exit
	syscall


msg   db    'Hello, Friend!',0xa
len   equ   $-msg

