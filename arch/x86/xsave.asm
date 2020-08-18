bits 64

global __fpu_xsave64
global __fpu_xrstor64

__fpu_xsave64:
	mov rdx, 0xFFFFFFFFFFFFFFFF
	mov rax, 0xFFFFFFFFFFFFFFFF
	xsave [rdi]
	ret


__fpu_xrstor64:
	mov rdx, 0xFFFFFFFFFFFFFFFF
	mov rax, 0xFFFFFFFFFFFFFFFF
	xrstor [rdi]
	ret
