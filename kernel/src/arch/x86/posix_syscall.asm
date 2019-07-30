

bits 64

global __posix_syscall

__posix_syscall:
	mov ax, 0
	mov dx, 0xfafa
	out dx, ax
	ret
