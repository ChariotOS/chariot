bits 64

global __fpu_xsave64
global __fpu_xrstor64

__fpu_xsave64:
	mov rdx, ~0
	mov rax, ~0
	xsave [rdi]
	ret


__fpu_xrstor64:
	mov rdx, ~0
	mov rax, ~0
	xrstor [rdi]
	ret
