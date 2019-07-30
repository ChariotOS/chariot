

bits 64

global do_posix_syscall

do_posix_syscall:
	mov ax, 0
	mov dx, 0xfafa
	out dx, ax
	ret
